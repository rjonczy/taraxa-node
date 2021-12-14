#include "network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

GetDagSyncPacketHandler::GetDagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<PacketsStats> packets_stats,
                                                 std::shared_ptr<TransactionManager> trx_mgr,
                                                 std::shared_ptr<DagManager> dag_mgr,
                                                 std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                                 std::shared_ptr<DbStorage> db, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      db_(std::move(db)) {}

void GetDagSyncPacketHandler::process(const PacketData &packet_data,
                                      [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  std::unordered_set<blk_hash_t> blocks_hashes;
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  auto it = packet_data.rlp_.begin();
  const auto peer_period = (*it++).toInt<uint64_t>();

  LOG(log_dg_) << "Received GetDagSyncPacket with " << packet_data.rlp_.itemCount() - 1 << " known blocks";

  for (; it != packet_data.rlp_.end(); ++it) {
    blocks_hashes.emplace(*it);
  }

  const auto [period, blocks] = dag_mgr_->getNonFinalizedBlocks();
  // There is no point in sending blocks if periods do not match
  if (peer_period == period) {
    peer->syncing_ = false;
    for (auto &level_blocks : blocks) {
      for (auto &block : level_blocks.second) {
        const auto hash = block;
        if (blocks_hashes.count(hash) == 0) {
          if (auto blk = dag_blk_mgr_->getDagBlock(hash); blk) {
            dag_blocks.emplace_back(blk);
          } else {
            LOG(log_er_) << "NonFinalizedBlock " << hash << " not in DB";
            assert(false);
          }
        }
      }
    }
  }
  sendBlocks(packet_data.from_node_id_, dag_blocks, peer_period, period);
}

void GetDagSyncPacketHandler::sendBlocks(dev::p2p::NodeID const &peer_id, std::vector<std::shared_ptr<DagBlock>> blocks,
                                         uint64_t request_period, uint64_t period) {
  auto peer = peers_state_->getPeer(peer_id);
  if (!peer) return;

  std::unordered_map<blk_hash_t, std::vector<taraxa::bytes>> block_transactions;
  size_t total_transactions_count = 0;
  for (const auto &block : blocks) {
    std::vector<taraxa::bytes> transactions;
    for (auto trx : block->getTrxs()) {
      auto t = trx_mgr_->getTransaction(trx);
      if (!t) {
        LOG(log_er_) << "Transacation " << trx << " is not available. SendBlocks canceled";
        // TODO: This can happen on stopping the node because network
        // is not stopped since network does not support restart,
        // better solution needed
        return;
      }
      transactions.emplace_back(std::move(*t->rlp()));
      total_transactions_count++;
    }
    LOG(log_nf_) << "Send DagBlock " << block->getHash() << "# Trx: " << transactions.size() << std::endl;
    block_transactions[block->getHash()] = std::move(transactions);
  }

  dev::RLPStream s(2 + blocks.size() + total_transactions_count);
  s << static_cast<uint64_t>(request_period);
  s << static_cast<uint64_t>(period);
  for (auto &block : blocks) {
    s.appendRaw(block->rlp(true));
    taraxa::bytes trx_bytes;
    for (auto &trx : block_transactions[block->getHash()]) {
      trx_bytes.insert(trx_bytes.end(), std::make_move_iterator(trx.begin()), std::make_move_iterator(trx.end()));
    }
    s.appendRaw(trx_bytes, block_transactions[block->getHash()].size());
  }
  sealAndSend(peer_id, SubprotocolPacketType::DagSyncPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap
