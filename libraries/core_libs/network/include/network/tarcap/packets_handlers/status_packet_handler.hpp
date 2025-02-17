#pragma once

#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"

namespace taraxa {
class NextVotesManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class StatusPacketHandler final : public ExtSyncingPacketHandler {
 public:
  StatusPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                      std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                      std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                      std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                      std::shared_ptr<NextVotesManager> next_votes_mgr, std::shared_ptr<DbStorage> db,
                      h256 genesis_hash, const addr_t& node_addr);

  bool sendStatus(const dev::p2p::NodeID& node_id, bool initial);
  void sendStatusToPeers();

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::StatusPacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  static constexpr uint16_t kInitialStatusPacketItemsCount = 12;
  static constexpr uint16_t kStandardStatusPacketItemsCount = 5;

  const h256 genesis_hash_;

  std::shared_ptr<NextVotesManager> next_votes_mgr_;
};

}  // namespace taraxa::network::tarcap
