#include "network/tarcap/packets_handlers/dag_sync_packet_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagSyncPacketHandler::DagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                           std::shared_ptr<PacketsStats> packets_stats,
                                           std::shared_ptr<SyncingState> syncing_state,
                                           std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                                           std::shared_ptr<DagManager> dag_mgr,
                                           std::shared_ptr<TransactionManager> trx_mgr,
                                           std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<DbStorage> db,
                                           const addr_t& node_addr)
    : ExtSyncingPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(dag_blk_mgr),
                              std::move(db), node_addr, "DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)) {}

void DagSyncPacketHandler::process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) {
  std::string received_dag_blocks_str;

  auto it = packet_data.rlp_.begin();

  const auto request_period = (*it++).toInt<uint64_t>();
  const auto response_period = (*it++).toInt<uint64_t>();

  // If the periods did not match restart syncing
  if (response_period > request_period) {
    LOG(log_nf_) << "Received DagSyncPacket with mismatching periods: " << response_period << " " << request_period;
    if (peer->pbft_chain_size_ < response_period) {
      peer->pbft_chain_size_ = response_period;
    }
    // DAG synced is set to true because this flag is used to prevent any possible ddos requesting
    peer->peer_dag_synced = true;
    peer->peer_dag_syncing = false;
    // We might be behind, restart pbft sync if needed
    restartSyncingPbft();
    return;
  } else if (response_period < request_period) {
    // This should not be possible for honest node
    LOG(log_wr_) << "Received DagSyncPacket with mismatching periods: " << response_period << " " << request_period;
    disconnect(peer->getId(), dev::p2p::UserReason);
    // TODO: malicious peer handling
    return;
  }

  for (; it != packet_data.rlp_.end();) {
    DagBlock block(*it++);
    peer->markDagBlockAsKnown(block.getHash());

    SharedTransactions new_transactions;
    for (size_t i = 0; i < block.getTrxs().size(); i++) {
      Transaction transaction(*it++);
      peer->markTransactionAsKnown(transaction.getHash());
      new_transactions.emplace_back(std::make_shared<Transaction>(std::move(transaction)));
    }

    received_dag_blocks_str += block.getHash().abridged() + " ";

    auto status = checkDagBlockValidation(block);
    if (!status.first) {
      // This should only happen with a malicious node or a fork
      LOG(log_er_) << "DagBlock" << block.getHash() << " Validation failed " << status.second << " . Peer "
                   << packet_data.from_node_id_.abridged() << " will be disconnected.";
      disconnect(peer->getId(), dev::p2p::UserReason);
      // TODO: malicious peer handling
      return;
    }

    LOG(log_dg_) << "Storing block " << block.getHash().abridged() << " with " << new_transactions.size()
                 << " transactions";
    if (block.getLevel() > peer->dag_level_) peer->dag_level_ = block.getLevel();

    trx_mgr_->insertBroadcastedTransactions(std::move(new_transactions));
    dag_blk_mgr_->insertBroadcastedBlock(std::move(block));
  }

  peer->peer_dag_synced = true;
  peer->peer_dag_syncing = false;

  LOG(log_nf_) << "Received DagSyncPacket with blocks: " << received_dag_blocks_str;
}

}  // namespace taraxa::network::tarcap
