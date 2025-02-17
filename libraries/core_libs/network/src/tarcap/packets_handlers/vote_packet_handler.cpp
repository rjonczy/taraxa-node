#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotePacketHandler::VotePacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                     std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                                     std::shared_ptr<VoteManager> vote_mgr, const addr_t &node_addr)
    : ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), node_addr, "PBFT_VOTE_PH") {}

void VotePacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  // Vote packet can contain either just a vote or vote + block + peer_chain_size
  if (items != kVotePacketSize && items != kExtendedVotePacketSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kExtendedVotePacketSize);
  }
}

void VotePacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  // Optional packet items
  std::shared_ptr<PbftBlock> pbft_block{nullptr};
  std::optional<uint64_t> peer_chain_size{};

  std::shared_ptr<Vote> vote = std::make_shared<Vote>(packet_data.rlp_[0]);
  if (const size_t item_count = packet_data.rlp_.itemCount(); item_count == kExtendedVotePacketSize) {
    pbft_block = std::make_shared<PbftBlock>(packet_data.rlp_[1]);
    peer_chain_size = packet_data.rlp_[2].toInt();
    LOG(log_dg_) << "Received PBFT vote " << vote->getHash() << " with PBFT block " << pbft_block->getBlockHash();
  } else {
    LOG(log_dg_) << "Received PBFT vote " << vote->getHash();
  }

  const auto vote_hash = vote->getHash();

  // Do not process vote that has already been validated
  if (vote_mgr_->voteAlreadyValidated(vote_hash)) {
    LOG(log_dg_) << "Received vote " << vote_hash << " has already been validated";
    return;
  }

  if (pbft_block) {
    if (pbft_block->getBlockHash() != vote->getBlockHash()) {
      std::ostringstream err_msg;
      err_msg << "Vote " << vote->getHash().abridged() << " voted block " << vote->getBlockHash().abridged()
              << " != actual block " << pbft_block->getBlockHash().abridged();
      throw MaliciousPeerException(err_msg.str());
    }

    peer->markPbftBlockAsKnown(pbft_block->getBlockHash());
  }

  if (vote->getPeriod() == current_pbft_period && (current_pbft_round - 1) == vote->getRound() &&
      vote->getType() == PbftVoteTypes::next_vote) {
    // Previous round next vote
    // We could switch round before other nodes, so we need to process also previous round next votes
    if (!processNextSyncVote(vote, pbft_block)) {
      return;
    }
  } else if (vote->getPeriod() >= current_pbft_period) {
    // Standard vote
    if (!processStandardVote(vote, pbft_block, peer, true)) {
      return;
    }

  } else if (vote->getPeriod() == current_pbft_period - 1 && vote->getType() == PbftVoteTypes::cert_vote) {
    // potential reward vote
    if (!processRewardVote(vote)) {
      return;
    }

  } else {
    // Too old vote
    LOG(log_dg_) << "Drop vote " << vote_hash << ". Vote period " << vote->getPeriod()
                 << " too old. current_pbft_period " << current_pbft_period;
    return;
  }

  // Do not mark it before, as peers have small caches of known votes. Only mark gossiping votes
  peer->markVoteAsKnown(vote_hash);
  onNewPbftVote(vote, pbft_block);

  // Update peer's max chain size
  if (peer_chain_size.has_value() && *peer_chain_size > peer->pbft_chain_size_) {
    peer->pbft_chain_size_ = *peer_chain_size;
  }
}

void VotePacketHandler::onNewPbftVote(const std::shared_ptr<Vote> &vote, const std::shared_ptr<PbftBlock> &block) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      LOG(log_dg_) << " PBFT vote " << vote->getHash() << " not sent to " << peer.first << " peer syncing";
      continue;
    }

    if (peer.second->isVoteKnown(vote->getHash())) {
      continue;
    }

    // Peer already has pbft block, do not send it (do not check it for propose votes as it could happen that nodes
    // re-propose the same block for new round, in which case we need to send the block again
    if (vote->getType() != PbftVoteTypes::propose_vote && peer.second->isPbftBlockKnown(vote->getBlockHash())) {
      sendPbftVote(peer.second, vote, nullptr);
    } else {
      sendPbftVote(peer.second, vote, block);
    }
  }
}

void VotePacketHandler::sendPbftVote(const std::shared_ptr<TaraxaPeer> &peer, const std::shared_ptr<Vote> &vote,
                                     const std::shared_ptr<PbftBlock> &block) {
  if (block && block->getBlockHash() != vote->getBlockHash()) {
    LOG(log_er_) << "Vote " << vote->getHash().abridged() << " voted block " << vote->getBlockHash().abridged()
                 << " != actual block " << block->getBlockHash().abridged();
    return;
  }

  dev::RLPStream s;

  if (block) {
    s = dev::RLPStream(kExtendedVotePacketSize);
    s.appendRaw(vote->rlp(true, false));
    s.appendRaw(block->rlp(true));
    s.append(pbft_chain_->getPbftChainSize());
  } else {
    s = dev::RLPStream(kVotePacketSize);
    s.appendRaw(vote->rlp(true, false));
  }

  if (sealAndSend(peer->getId(), SubprotocolPacketType::VotePacket, std::move(s))) {
    peer->markVoteAsKnown(vote->getHash());
    if (block) {
      peer->markPbftBlockAsKnown(block->getBlockHash());
      LOG(log_dg_) << " PBFT vote " << vote->getHash() << " together with block " << block->getBlockHash()
                   << " sent to " << peer->getId();
    } else {
      LOG(log_dg_) << " PBFT vote " << vote->getHash() << " sent to " << peer->getId();
    }
  }
}

}  // namespace taraxa::network::tarcap
