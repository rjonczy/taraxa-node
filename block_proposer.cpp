#include "block_proposer.hpp"
#include <cmath>
#include "dag.hpp"
#include "full_node.hpp"
#include "transaction.hpp"
#include "types.hpp"
namespace taraxa {

std::atomic<uint64_t> BlockProposer::num_proposed_blocks = 0;
std::mt19937 RandomPropose::generator;

uint posLog2(val_t val) {
  if (val <= 0) return 0;
  int count = 0;
  while (val) {
    val >>= 1;
    count += 1;
  }
  return count - 1;
}

bool RandomPropose::propose() {
  auto proposer = proposer_.lock();
  if (!proposer) {
    LOG(log_er_) << "Block proposer not available" << std::endl;
    return false;
  }
  auto delay = distribution_(RandomPropose::generator);
  thisThreadSleepForMilliSeconds(delay);
  LOG(log_tr_) << "Add proposer delay " << delay << std::endl;

  std::string pivot;
  std::vector<std::string> tips;
  vec_trx_t sharded_trxs;

  bool ok = proposer->getShardedTrxs(sharded_trxs);
  if (!ok) {
    return false;
  }
  ok = proposer->getLatestPivotAndTips(pivot, tips);
  if (!ok) {
    return false;
  }

  vec_blk_t tip_hashes;
  for (auto const& t : tips) {
    tip_hashes.emplace_back(blk_hash_t(t));
  }
  auto pivot_hash = blk_hash_t(pivot);
  auto propose_level = proposer->getProposeLevel(pivot_hash, tip_hashes) + 1;

  DagBlock blk(pivot_hash, propose_level, tip_hashes, sharded_trxs);

  proposer_.lock()->proposeBlock(blk);
  return true;
}

bool SortitionPropose::propose() {
  thisThreadSleepForMilliSeconds(propose_interval_);
  auto proposer = proposer_.lock();
  bool ret = false;
  if (!proposer) {
    LOG(log_er_) << "Block proposer not available" << std::endl;
    return false;
  }
  std::string pivot;
  std::vector<std::string> tips;

  bool ok = proposer->getLatestPivotAndTips(pivot, tips);
  if (ok) {
    LOG(log_nf_) << "BlockProposer: pivot: " << pivot
                 << ", tip size = " << tips.size() << std::endl;
    LOG(log_nf_) << "Tips: " << tips;
  } else {
    LOG(log_er_) << "Pivot and tips unavailable ..." << std::endl;
    return false;
  }
  vec_blk_t tip_hashes;
  for (auto const& t : tips) {
    tip_hashes.emplace_back(blk_hash_t(t));
  }
  auto pivot_hash = blk_hash_t(pivot);
  auto propose_level = proposer->getProposeLevel(pivot_hash, tip_hashes) + 1;

  // get sortition

  bool win = proposer->winProposeSortition(propose_level, threshold_);
  if (win) {
    vec_trx_t sharded_trxs;
    ok = proposer->getShardedTrxs(sharded_trxs);
    if (!ok) {
      LOG(log_dg_) << "Cannot get transactions ... ";
      return false;
    }
    DagBlock blk(pivot_hash, propose_level, tip_hashes, sharded_trxs);
    proposer_.lock()->proposeBlock(blk);
    ret = true;
  }
  return ret;
}

std::shared_ptr<BlockProposer> BlockProposer::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr& e) {
    std::cerr << "FullNode: " << e.what() << std::endl;
    return nullptr;
  }
}

void BlockProposer::start() {
  if (!stopped_) return;
  if (full_node_.expired()) {
    LOG(log_er_) << "FullNode is not set ..." << std::endl;
    return;
  }

  LOG(log_nf_) << "BlockProposer started ..." << std::endl;
  stopped_ = false;
  // reset number of proposed blocks
  BlockProposer::num_proposed_blocks = 0;
  propose_model_->setProposer(getShared(), full_node_.lock()->getSecretKey());
  proposer_worker_ = std::make_shared<std::thread>([this]() {
    while (!stopped_) {
      propose_model_->propose();
    }
  });
}
void BlockProposer::stop() {
  if (stopped_) return;
  stopped_ = true;
  proposer_worker_->join();
}

void BlockProposer::setFullNode(std::shared_ptr<FullNode> full_node) {
  full_node_ = full_node;
  auto addr = std::stoull(
      full_node->getAddress().toString().substr(0, 6).c_str(), NULL, 16);
  my_trx_shard_ = addr % conf_.shard;
  LOG(log_nf_) << "Block proposer in " << my_trx_shard_ << " shard ...";
}

bool BlockProposer::getLatestPivotAndTips(std::string& pivot,
                                          std::vector<std::string>& tips) {
  auto dag_mgr = dag_mgr_.lock();
  if (!dag_mgr) {
    LOG(log_wr_) << "DagManager expired ..." << std::endl;
    return false;
  }
  bool ok = dag_mgr->getLatestPivotAndTips(pivot, tips);
  if (ok) {
    LOG(log_nf_) << "BlockProposer: pivot: " << pivot
                 << ", tip size = " << tips.size() << std::endl;
    LOG(log_nf_) << "Tips: " << tips;
  } else {
    LOG(log_er_) << "Pivot and tips unavailable ..." << std::endl;
    return ok;
  }
  auto& log_time = full_node_.lock()->getTimeLogger();

  LOG(log_time) << "Pivot and Tips retrieved at: "
                << getCurrentTimeMilliSeconds();
  return ok;
}

bool BlockProposer::getShardedTrxs(uint total_shard, uint my_shard,
                                   vec_trx_t& sharded_trxs) {
  auto full_node = full_node_.lock();
  if (!full_node) {
    LOG(log_er_) << "FullNode expired ..." << std::endl;
    return false;
  }
  auto& log_time = full_node->getTimeLogger();
  vec_trx_t to_be_packed_trx;
  auto trx_mgr = trx_mgr_.lock();
  if (!trx_mgr) {
    LOG(log_er_) << "TransactionManager expired ..." << std::endl;
    return false;
  }
  trx_mgr->packTrxs(to_be_packed_trx);
  if (to_be_packed_trx.empty()) {
    LOG(log_dg_) << "Skip block proposer, zero unpacked transactions ..."
                 << std::endl;
    return false;
  }
  sharded_trxs.clear();
  for (auto const& t : to_be_packed_trx) {
    auto shard = std::stoull(t.toString().substr(0, 10), NULL, 16);
    if (shard % total_trx_shards_ == my_trx_shard_) {
      sharded_trxs.emplace_back(t);
    }
  }
  if (sharded_trxs.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero sharded transactions ..."
                 << std::endl;
    return false;
  }
  return true;
}

level_t BlockProposer::getProposeLevel(blk_hash_t const& pivot,
                                       vec_blk_t const& tips) {
  level_t max_level = 0;
  auto full_node = full_node_.lock();
  if (!full_node) {
    LOG(log_er_) << "FullNode expired, cannot get propose level ..."
                 << std::endl;
    return 0;
  }
  // get current level
  auto pivot_blk = full_node->getDagBlock(pivot);
  if (!pivot_blk) {
    LOG(log_er_) << "Cannot find pivot dag block " << pivot;
    return 0;
  }
  max_level = std::max(pivot_blk->getLevel(), max_level);

  for (auto const& t : tips) {
    auto tip_blk = full_node->getDagBlock(t);
    if (!tip_blk) {
      LOG(log_er_) << "Cannot find tip dag block " << blk_hash_t(t);
      return 0;
    }
    max_level = std::max(tip_blk->getLevel(), max_level);
  }
  return max_level;
}
bool BlockProposer::winProposeSortition(level_t propose_level,
                                        uint64_t threshold) {
  bool ret = false;
  auto full_node = full_node_.lock();
  auto anchor = full_node->getLatestAnchor();
  auto message = anchor.toString() + std::to_string(propose_level);
  uint64_t ticket = uint64_t(
      uint256_t(dev::sha3(full_node->signMessage(message))) >> 210);  // 46 bits
  uint64_t beta = (full_node->getBlockProposeThresholdBeta());        // 10 bits
  auto my_bal = full_node->getMyBalance();  //  0 ~ 2^53 - 1
  if (my_bal == 0) {
    LOG(log_dg_) << "Cannot win ticket, " << getFullNodeAddress()
                 << " balance is 0 ...";
    return false;
  }
  auto log_bal = posLog2(my_bal) + 1;              // 1~16, 6 bits
  auto my_threshold = log_bal * beta * threshold;  // 46 bits
  if (ticket < my_threshold) {
    LOG(log_dg_) << "Win sortition at level: " << propose_level
                 << " , ticket = " << ticket
                 << " , threshold = " << my_threshold
                 << " , diff (%) = " << double(my_threshold - ticket) / ticket
                 << " , log_bal = " << log_bal << " , beta = " << beta
                 << " , threshold = " << threshold;
    ret = true;
  } else {
    LOG(log_dg_) << "Loose sortition at level: " << propose_level
                 << " , ticket = " << ticket
                 << " , threshold = " << my_threshold
                 << " , diff (%) = " << double(my_threshold - ticket) / ticket
                 << " , log_bal = " << log_bal << " , beta = " << beta
                 << " , threshold = " << threshold;
  }
  return ret;
}  // namespace taraxa
void BlockProposer::proposeBlock(DagBlock& blk) {
  auto full_node = full_node_.lock();
  assert(full_node);

  blk.sign(full_node->getSecretKey());
  full_node_.lock()->insertBlock(blk);

  auto& log_time = full_node->getTimeLogger();
  auto now = getCurrentTimeMilliSeconds();

  LOG(log_time) << "Propose block " << blk.getHash() << " at: " << now
                << " ,trxs: " << blk.getTrxs()
                << " , tips: " << blk.getTips().size();
  LOG(log_si_) << getFullNodeAddress() << " Propose block :" << blk.getHash()
               << " , number of trx (" << blk.getTrxs().size() << ")";
  BlockProposer::num_proposed_blocks.fetch_add(1);
}

addr_t BlockProposer::getFullNodeAddress() const {
  auto full_node = full_node_.lock();
  if (full_node) {
    return full_node->getAddress();
  } else {
    return addr_t();
  }
}

}  // namespace taraxa
