#pragma once

#include "dag/dag_block.hpp"
#include "libp2p/Common.h"
#include "libp2p/Host.h"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/packets_handler/packets_stats/packets_stats.hpp"
#include "taraxa_peer.hpp"
#include "transaction_manager/transaction.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief PeersState contains common members and functions related to peers, host, etc... that are shared among multiple
 * classes
 */
class PeersState {
 public:
  PeersState(std::weak_ptr<dev::p2p::Host>&& host);

  std::shared_ptr<TaraxaPeer> getPeer(const dev::p2p::NodeID& node_id);
  std::shared_ptr<TaraxaPeer> getPendingPeer(const dev::p2p::NodeID& node_id);
  size_t getPeersCount();
  std::shared_ptr<TaraxaPeer> addPendingPeer(const dev::p2p::NodeID& node_id);
  void erasePeer(const dev::p2p::NodeID& node_id);
  std::shared_ptr<TaraxaPeer> setPeerAsReadyToSendMessages(dev::p2p::NodeID const& node_id,
                                                           std::shared_ptr<TaraxaPeer> peer);

  // TODO: why dev::RLPStream and not const & or && ???
  bool sealAndSend(const dev::p2p::NodeID& nodeID, SubprotocolPacketType packet_type, dev::RLPStream rlp);

 public:
  static constexpr uint32_t MAX_PACKET_SIZE = 15 * 1024 * 1024;  // 15 MB -> 15 * 1024 * 1024 B

  // TODO: why weak_ptr ?
  std::weak_ptr<dev::p2p::Host> host_;
  dev::p2p::NodeID node_id_;

  mutable std::shared_mutex peers_mutex_;
  std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> peers_;
  std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> pending_peers_;

  // Shared packet stats
  PacketsStats packets_stats_;

  // FOR TESTING ONLY
  std::unordered_map<blk_hash_t, DagBlock> test_blocks_;
  std::unordered_map<trx_hash_t, Transaction> test_transactions_;
};

}  // namespace taraxa::network::tarcap
