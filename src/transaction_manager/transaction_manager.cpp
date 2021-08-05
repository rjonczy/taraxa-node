#include "transaction_manager.hpp"

#include <string>
#include <utility>

#include "dag/dag.hpp"
#include "logger/log.hpp"
#include "network/network.hpp"
#include "network/rpc/WSServer.h"
#include "transaction.hpp"

using namespace taraxa::net;
namespace taraxa {
auto trxComp = [](Transaction const &t1, Transaction const &t2) -> bool {
  if (t1.getSender() < t2.getSender())
    return true;
  else if (t1.getSender() == t2.getSender()) {
    return t1.getNonce() < t2.getNonce();
  } else {
    return false;
  }
};

TransactionManager::TransactionManager(FullNodeConfig const &conf, addr_t node_addr, std::shared_ptr<DbStorage> db,
                                       logger::Logger log_time)
    : db_(db), conf_(conf), trx_qu_(node_addr), node_addr_(node_addr), log_time_(log_time) {
  LOG_OBJECTS_CREATE("TRXMGR");
  auto trx_count = db_->getStatusField(taraxa::StatusDbField::TrxCount);
  trx_count_.store(trx_count);
}

std::pair<bool, std::string> TransactionManager::verifyTransaction(Transaction const &trx) const {
  if (trx.getChainID() != conf_.chain.chain_id) {
    return {false, "chain_id mismatch"};
  }
  try {
    trx.getSender();
  } catch (Transaction::InvalidSignature const &) {
    return {false, "invalid signature"};
  }
  return {true, ""};
}

bool TransactionManager::checkQueueOverflow() {
  const auto queues_sizes = trx_qu_.getTransactionQueueSize();
  size_t combined_queues_size =
      queues_sizes.first /* unverified txs queue */ + queues_sizes.second /* verified txs queue */;

  size_t queue_overflow_warn = conf_.test_params.max_transaction_queue_warn;
  size_t queue_overflow_drop = conf_.test_params.max_transaction_queue_drop;

  auto queuesOverflowWarnMsg = [&combined_queues_size, &queues_sizes, &queue_overflow_drop]() -> std::string {
    std::ostringstream oss;
    oss << "Warning: queues too large: "
        << "Combined queues size: " << combined_queues_size << ", Unverified queue: " << queues_sizes.first
        << ", Verified queue: " << queues_sizes.second << ", Limit: " << queue_overflow_drop;

    return oss.str();
  };

  if (queue_overflow_drop && combined_queues_size >= queue_overflow_drop) {
    LOG(log_wr_) << queuesOverflowWarnMsg() << " --> New transactions will not be processed !";
    return true;
  } else if (queue_overflow_warn && combined_queues_size >= queue_overflow_warn) {
    LOG(log_wr_) << queuesOverflowWarnMsg() << " --> Only warning, transactions will bed processed.";
  }

  return false;
}

std::pair<bool, std::string> TransactionManager::insertTransaction(const Transaction &trx, bool verify,
                                                                   bool broadcast) {
  if (checkQueueOverflow() == true) {
    LOG(log_er_) << "Queue overlfow";
    return std::make_pair(false, "Queue overlfow");
  }

  if (verify && mode_ != VerifyMode::skip_verify_sig) {
    if (const auto verified = verifyTransaction(trx); verified.first == false) {
      LOG(log_er_) << "Trying to insert invalid trx, hash: " << trx.getHash() << ", err msg: " << verified.second;
      return verified;
    }
  }

  auto hash = trx.getHash();

  TransactionStatus status = db_->getTransactionStatus(hash);
  if (status != TransactionStatus::not_seen) {
    switch (status) {
      case TransactionStatus::in_queue_verified:
        LOG(log_dg_) << "Trx: " << hash << "skip, seen in verified queue. " << std::endl;
        return std::make_pair(false, "in verified queue");
      case TransactionStatus::in_queue_unverified:
        LOG(log_dg_) << "Trx: " << hash << "skip, seen in unverified queue. " << std::endl;
        return std::make_pair(false, "in unverified queue");
      case TransactionStatus::in_block:
        LOG(log_dg_) << "Trx: " << hash << "skip, seen in block. " << std::endl;
        return std::make_pair(false, "in block");
      case TransactionStatus::invalid:
        LOG(log_dg_) << "Trx: " << hash << "skip, seen but invalid. " << std::endl;
        return std::make_pair(false, "already invalid");
      default:
        return std::make_pair(false, "unknown");
    }
  }

  status = verify ? TransactionStatus::in_queue_verified : TransactionStatus::in_queue_unverified;
  db_->saveTransaction(trx, verify && mode_ != VerifyMode::skip_verify_sig);
  db_->saveTransactionStatus(hash, status);
  trx_qu_.insert(trx, verify);

  if (ws_server_) ws_server_->newPendingTransaction(trx.getHash());

  if (broadcast == true) {
    if (auto net = network_.lock(); net && conf_.network.network_transaction_interval == 0) {
      net->onNewTransactions({*trx.rlp()});
    }
  }

  return std::make_pair(true, "");
}

uint32_t TransactionManager::insertBroadcastedTransactions(const std::vector<taraxa::bytes> &raw_trxs) {
  if (stopped_) {
    return 0;
  }

  if (checkQueueOverflow() == true) {
    return 0;
  }

  std::vector<trx_hash_t> trxs_hashes;
  std::vector<Transaction> trxs;
  std::vector<Transaction> unseen_trxs;

  trxs_hashes.reserve(raw_trxs.size());
  trxs.reserve(raw_trxs.size());
  unseen_trxs.reserve(raw_trxs.size());

  for (const auto &raw_trx : raw_trxs) {
    Transaction trx = Transaction(raw_trx);

    trxs_hashes.push_back(trx.getHash());
    trxs.push_back(std::move(trx));
  }

  // Get transactions statuses from db
  DbStorage::MultiGetQuery db_query(db_, trxs_hashes.size());
  db_query.append(DbStorage::Columns::trx_status, trxs_hashes);
  auto db_trxs_statuses = db_query.execute();

  auto write_batch = db_->createWriteBatch();
  for (size_t idx = 0; idx < db_trxs_statuses.size(); idx++) {
    auto &trx_raw_status = db_trxs_statuses[idx];
    const trx_hash_t &trx_hash = trxs_hashes[idx];

    TransactionStatus trx_status =
        trx_raw_status.empty() ? TransactionStatus::not_seen : (TransactionStatus) * (uint16_t *)&trx_raw_status[0];
    // TransactionStatus trx_status = trx_raw_status.empty() ? TransactionStatus::not_seen :
    // static_cast<TransactionStatus>(*reinterpret_cast<uint16_t*>(&trx_raw_status[0]));

    LOG(log_dg_) << "Broadcasted transaction " << trx_hash << " received at: " << getCurrentTimeMilliSeconds();

    // Trx status was already saved in db -> it means we already processed this trx
    // Do not process it again
    if (trx_status != TransactionStatus::not_seen) {
      switch (trx_status) {
        case TransactionStatus::in_queue_verified:
          LOG(log_dg_) << "Trx: " << trx_hash << " skipped, seen in verified queue.";
          break;
        case TransactionStatus::in_queue_unverified:
          LOG(log_dg_) << "Trx: " << trx_hash << " skipped, seen in unverified queue.";
          break;
        case TransactionStatus::in_block:
          LOG(log_dg_) << "Trx: " << trx_hash << " skipped, seen in block.";
          break;
        case TransactionStatus::invalid:
          LOG(log_dg_) << "Trx: " << trx_hash << " skipped, seen but invalid.";
          break;
        default:
          LOG(log_dg_) << "Trx: " << trx_hash << " skipped, unknown trx status saved in db.";
      }

      continue;
    }

    const Transaction &trx = trxs[idx];

    db_->addTransactionToBatch(trx, write_batch);
    db_->addTransactionStatusToBatch(write_batch, trx_hash, TransactionStatus::in_queue_unverified);

    unseen_trxs.push_back(std::move(trx));
    if (ws_server_) ws_server_->newPendingTransaction(trx_hash);
  }

  db_->commitWriteBatch(write_batch);
  trx_qu_.insertUnverifiedTrxs(unseen_trxs);

  LOG(log_nf_) << raw_trxs.size() << " received txs processed (" << unseen_trxs.size()
               << " unseen -> inserted into db).";
  return unseen_trxs.size();
}

void TransactionManager::verifyQueuedTrxs() {
  while (!stopped_) {
    // It will wait if no transaction in unverified queue
    auto item = trx_qu_.getUnverifiedTransaction();
    if (stopped_) return;

    std::pair<bool, std::string> valid;
    trx_hash_t hash = item.second->getHash();
    // verify and put the transaction to verified queue
    if (mode_ == VerifyMode::skip_verify_sig) {
      valid.first = true;
    } else {
      valid = verifyTransaction(*item.second);
    }
    // mark invalid
    if (!valid.first) {
      db_->saveTransactionStatus(hash, TransactionStatus::invalid);
      trx_qu_.removeTransactionFromBuffer(hash);

      LOG(log_wr_) << " Trx: " << hash << "invalid: " << valid.second << std::endl;
      continue;
    }
    {
      auto status = db_->getTransactionStatus(hash);
      if (status == TransactionStatus::in_queue_unverified) {
        db_->saveTransactionStatus(hash, TransactionStatus::in_queue_verified);
        db_->saveTransaction(*item.second, mode_ != VerifyMode::skip_verify_sig);
        trx_qu_.addTransactionToVerifiedQueue(hash, item.second);
      }
    }
  }
}

void TransactionManager::setNetwork(std::weak_ptr<Network> network) { network_ = move(network); }

void TransactionManager::setWsServer(std::shared_ptr<WSServer> ws_server) { ws_server_ = ws_server; }

void TransactionManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  trx_qu_.start();
  verifiers_.clear();
  for (size_t i = 0; i < num_verifiers_; ++i) {
    LOG(log_nf_) << "Create Transaction verifier ... " << std::endl;
    verifiers_.emplace_back([this]() { verifyQueuedTrxs(); });
  }
  assert(num_verifiers_ == verifiers_.size());
}

void TransactionManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  trx_qu_.stop();
  for (auto &t : verifiers_) {
    t.join();
  }
}

std::unordered_map<trx_hash_t, Transaction> TransactionManager::getVerifiedTrxSnapShot() const {
  return trx_qu_.getVerifiedTrxSnapShot();
}

std::pair<size_t, size_t> TransactionManager::getTransactionQueueSize() const {
  return trx_qu_.getTransactionQueueSize();
}

size_t TransactionManager::getTransactionBufferSize() const { return trx_qu_.getTransactionBufferSize(); }

std::vector<taraxa::bytes> TransactionManager::getNewVerifiedTrxSnapShotSerialized() {
  auto verified_trxs = trx_qu_.getNewVerifiedTrxSnapShot();
  std::vector<Transaction> vec_trxs;
  std::copy(verified_trxs.begin(), verified_trxs.end(), std::back_inserter(vec_trxs));
  sort(vec_trxs.begin(), vec_trxs.end(), trxComp);
  std::vector<taraxa::bytes> ret;
  for (auto const &t : vec_trxs) {
    ret.emplace_back(*t.rlp());
  }
  return ret;
}

unsigned long TransactionManager::getTransactionCount() const { return trx_count_; }

void TransactionManager::addTrxCount(unsigned long num) { trx_count_ += num; }

std::shared_ptr<std::pair<Transaction, taraxa::bytes>> TransactionManager::getTransaction(
    trx_hash_t const &hash) const {
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> tr;
  auto t = trx_qu_.getTransaction(hash);
  if (t) {  // find in queue
    tr = std::make_shared<std::pair<Transaction, taraxa::bytes>>(std::make_pair(*t, *t->rlp()));
  } else {  // search from db
    tr = db_->getTransactionExt(hash);
  }
  return tr;
}

/**
 * This is for block proposer
 * Few steps:
 * 1. get a snapshot (move) of verified queue (lock)
 *	  now, verified trxs can include (A) unpacked ,(B) packed by other ,(C)
 *old trx that only seen in db
 * 2. write A, B to database, of course C will be rejected (already exist in
 *database)
 * 3. propose transactions for block A
 * 4. update A, B and C status to seen_in_db
 */
void TransactionManager::packTrxs(vec_trx_t &to_be_packed_trx, uint16_t max_trx_to_pack) {
  to_be_packed_trx.clear();
  std::list<Transaction> list_trxs;

  auto verified_trx = trx_qu_.moveVerifiedTrxSnapShot(max_trx_to_pack);

  auto trx_batch = db_->createWriteBatch();
  vector<h256> accepted_trx_hashes;
  accepted_trx_hashes.reserve(verified_trx.size());
  {
    for (auto const &i : verified_trx) {
      trx_hash_t const &hash = i.first;
      Transaction const &trx = i.second;
      auto status = db_->getTransactionStatus(hash);
      if (status == TransactionStatus::in_queue_verified) {
        // Skip if transaction is already in existing block
        db_->addTransactionStatusToBatch(trx_batch, hash, TransactionStatus::in_block);
        trx_count_.fetch_add(1);
        accepted_trx_hashes.emplace_back(hash);
        LOG(log_dg_) << "Trx: " << hash << " ready to pack" << std::endl;
        // update transaction_status
        list_trxs.push_back(trx);
      }
    }

    if (!accepted_trx_hashes.empty()) {
      auto trx_count = trx_count_.load();
      db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count, trx_batch);
      db_->commitWriteBatch(trx_batch);
      for (auto const &h : accepted_trx_hashes) {
        transaction_accepted_.emit(h);
      }
    }
  }

  if (list_trxs.size() == 0) {
    return;
  }

  // sort trx based on sender and nonce
  list_trxs.sort(trxComp);

  std::transform(list_trxs.begin(), list_trxs.end(), std::back_inserter(to_be_packed_trx),
                 [](Transaction const &t) { return t.getHash(); });
}

// Save the transaction that came with the block together with the
// transactions that are in the queue. This will update the transaction
// status as well and remove the transactions from the queue
bool TransactionManager::verifyBlockTransactions(DagBlock const &blk, std::vector<Transaction> const &trxs) {
  vec_trx_t const &all_block_trx_hashes = blk.getTrxs();
  if (all_block_trx_hashes.empty()) {
    LOG(log_er_) << "Ignore block " << blk.getHash() << " since it has no transactions";
    return false;
  }

  DbStorage::MultiGetQuery db_query(db_, all_block_trx_hashes.size());
  db_query.append(DbStorage::Columns::trx_status, all_block_trx_hashes);
  auto db_trxs_statuses = db_query.execute(false);

  std::unordered_map<trx_hash_t, Transaction> known_trx(trxs.size());

  std::transform(trxs.begin(), trxs.end(), std::inserter(known_trx, known_trx.end()),
                 [](Transaction const &t) { return std::make_pair(t.getHash(), t); });

  bool all_transactions_saved = true;
  trx_hash_t missing_trx;
  auto trx_batch = db_->createWriteBatch();
  for (size_t idx = 0; idx < db_trxs_statuses.size(); ++idx) {
    const TransactionStatus &status = db_trxs_statuses[idx].empty()
                                          ? TransactionStatus::not_seen
                                          : (TransactionStatus) * (uint16_t *)&db_trxs_statuses[idx][0];

    if (status == TransactionStatus::in_queue_unverified || status == TransactionStatus::not_seen) {
      const trx_hash_t &trx_hash = all_block_trx_hashes[idx];
      if (known_trx.count(trx_hash)) {
        if (const auto valid = verifyTransaction(known_trx[trx_hash]); !valid.first) {
          LOG(log_er_) << "Block " << blk.getHash() << " has invalid transaction " << trx_hash.toString() << " "
                       << valid.second;
          return false;
        }
        db_->addTransactionToBatch(known_trx[trx_hash], trx_batch, true);

      } else if (status == TransactionStatus::in_queue_unverified) {
        auto tx = db_->getTransaction(trx_hash);
        if (const auto valid = verifyTransaction(*tx); !valid.first) {
          LOG(log_er_) << "Block " << blk.getHash() << " has invalid transaction " << trx_hash.toString() << " "
                       << valid.second;
          return false;
        }
        db_->addTransactionToBatch(*tx, trx_batch, true);

      } else {
        all_transactions_saved = false;
        missing_trx = trx_hash;
        break;
      }
    }
  }

  db_->commitWriteBatch(trx_batch);

  if (all_transactions_saved) {
    size_t newly_added_txs_to_block_counter = 0;
    trx_batch = db_->createWriteBatch();
    std::vector<trx_hash_t> accepted_trx_hashes;
    accepted_trx_hashes.reserve(all_block_trx_hashes.size());
    db_trxs_statuses = db_query.execute();
    for (size_t idx = 0; idx < db_trxs_statuses.size(); ++idx) {
      const TransactionStatus &status = db_trxs_statuses[idx].empty()
                                            ? TransactionStatus::not_seen
                                            : (TransactionStatus) * (uint16_t *)&db_trxs_statuses[idx][0];
      if (status != TransactionStatus::in_block) {
        const trx_hash_t &trx_hash = all_block_trx_hashes[idx];
        newly_added_txs_to_block_counter++;
        accepted_trx_hashes.push_back(trx_hash);
        db_->addTransactionStatusToBatch(trx_batch, trx_hash, TransactionStatus::in_block);
      }
    }
    // Write prepared batch to db
    trx_count_ += newly_added_txs_to_block_counter;
    db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count_, trx_batch);

    db_->commitWriteBatch(trx_batch);
    for (auto const &h : accepted_trx_hashes) {
      transaction_accepted_.emit(h);
    }
    trx_qu_.removeBlockTransactionsFromQueue(all_block_trx_hashes);
  } else {
    LOG(log_er_) << "Block " << blk.getHash() << " has missing transaction " << missing_trx;
  }

  return all_transactions_saved;
}

}  // namespace taraxa
