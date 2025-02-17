#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotesSyncPacketHandler::VotesSyncPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                               std::shared_ptr<PbftManager> pbft_mgr,
                                               std::shared_ptr<PbftChain> pbft_chain,
                                               std::shared_ptr<VoteManager> vote_mgr,
                                               std::shared_ptr<NextVotesManager> next_votes_mgr,
                                               std::shared_ptr<DbStorage> db, const addr_t &node_addr)
    : ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), node_addr, "VOTES_SYNC_PH"),
      next_votes_mgr_(std::move(next_votes_mgr)),
      db_(std::move(db)) {}

void VotesSyncPacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxVotesInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxVotesInPacket);
  }
}

void VotesSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto reference_vote = std::make_shared<Vote>(packet_data.rlp_[0]);

  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();
  const auto votes_bundle_pbft_period = reference_vote->getPeriod();
  const auto votes_bundle_pbft_round = reference_vote->getRound();
  const auto votes_bundle_votes_type = reference_vote->getType();
  const auto votes_bundle_voted_block = reference_vote->getBlockHash();

  // Accept only votes, which period is >= current pbft period - 1 (reward votes period)
  if (votes_bundle_pbft_period < current_pbft_period - 1) {
    LOG(log_wr_) << "Dropping votes sync packet due to period. Votes period: " << votes_bundle_pbft_period
                 << ", current pbft period: " << current_pbft_period;
    return;
  } else if (votes_bundle_pbft_period == current_pbft_period) {
    if (votes_bundle_pbft_round < current_pbft_round - 1) {
      // Accept only votes, which round is >= previous round(current pbft round - 1) in case their period == current
      // pbft period
      LOG(log_wr_) << "Dropping votes sync packet due to round. Votes round: " << votes_bundle_pbft_round
                   << ", current pbft round: " << current_pbft_round;
      return;
    } else if (votes_bundle_pbft_round == current_pbft_round - 1 &&
               votes_bundle_votes_type == PbftVoteTypes::next_vote) {
      // Already have 2t+1 previous round next votes for both kNullBlockHash as well as some specific block hash
      if (next_votes_mgr_->enoughNextVotes()) {
        LOG(log_nf_) << "Dropping next votes sync packet - already have enough next votes for previous round";
        return;
      }
    }
  }

  // VotesSyncPacket does not support propose votes
  if (votes_bundle_votes_type == PbftVoteTypes::propose_vote) {
    LOG(log_er_) << "Dropping votes sync packet due to received \"propose_votes\" votes from "
                 << packet_data.from_node_id_ << ". The peer may be a malicious player, will be disconnected";
    disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
    return;
  }

  std::vector<std::shared_ptr<Vote>> votes;
  blk_hash_t next_votes_bundle_voted_block = kNullBlockHash;

  const auto next_votes_count = packet_data.rlp_.itemCount();
  //  It is done in separate cycle because we don't need to process this next_votes if some of checks will fail
  for (size_t i = 0; i < next_votes_count; i++) {
    auto vote = std::make_shared<Vote>(packet_data.rlp_[i]);
    peer->markVoteAsKnown(vote->getHash());

    // Do not process vote that has already been validated
    if (vote_mgr_->voteAlreadyValidated(vote->getHash())) {
      LOG(log_dg_) << "Received vote " << vote->getHash() << " has already been validated";
      return;
    }

    // Next votes bundle can contain votes for kNullBlockHash as well as some specific block hash
    // TODO[2047]: when implementing issue 2047, check if this is correct -> we are sending all next votes so
    //             there could be potentially multiple voted blocks ???
    if (vote->getType() == PbftVoteTypes::next_vote) {
      if (next_votes_bundle_voted_block == kNullBlockHash && vote->getBlockHash() != kNullBlockHash) {
        // initialize voted value with first block hash not equal to kNullBlockHash
        next_votes_bundle_voted_block = vote->getBlockHash();
      }

      if (vote->getBlockHash() != kNullBlockHash && vote->getBlockHash() != next_votes_bundle_voted_block) {
        // we see different voted value, so bundle is invalid
        LOG(log_er_) << "Received next votes bundle with unmatched voted values(" << next_votes_bundle_voted_block
                     << ", " << vote->getBlockHash() << ") from " << packet_data.from_node_id_
                     << ". The peer may be a malicious player, will be disconnected";
        disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
        return;
      }
    } else {
      // Other votes bundles can contain votes only for 1 specific block hash
      if (vote->getBlockHash() != votes_bundle_voted_block) {
        // we see different voted value, so bundle is invalid
        LOG(log_er_) << "Received votes bundle with unmatched voted values(" << votes_bundle_voted_block << ", "
                     << vote->getBlockHash() << ") from " << packet_data.from_node_id_
                     << ". The peer may be a malicious player, will be disconnected";
        disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
        return;
      }
    }

    if (vote->getType() != votes_bundle_votes_type) {
      LOG(log_er_) << "Received votes bundle with unmatched types from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (vote->getPeriod() != votes_bundle_pbft_period) {
      LOG(log_er_) << "Received votes bundle with unmatched periods from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (vote->getRound() != votes_bundle_pbft_round) {
      LOG(log_er_) << "Received votes bundle with unmatched rounds from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    LOG(log_dg_) << "Received sync vote " << vote->getHash().abridged();

    // Previous round next vote
    if (votes_bundle_votes_type == PbftVoteTypes::next_vote && votes_bundle_pbft_period == current_pbft_period &&
        votes_bundle_pbft_round == (current_pbft_round - 1)) {
      if (!processNextSyncVote(vote, nullptr)) {
        continue;
      }
    } else if (votes_bundle_pbft_period >= current_pbft_period) {
      // Standard vote

      // Process processStandardVote is called with false in case of next votes bundle -> does not check max boundaries
      // for round and step to actually being able to sync the current round in case network is stalled
      bool check_max_round_step = votes_bundle_votes_type == PbftVoteTypes::next_vote ? false : true;
      if (!processStandardVote(vote, nullptr, peer, check_max_round_step)) {
        continue;
      }
    } else if (votes_bundle_votes_type == PbftVoteTypes::cert_vote &&
               votes_bundle_pbft_period == current_pbft_period - 1) {
      // Potential reward vote
      if (!processRewardVote(vote)) {
        continue;
      }
    } else {
      // Too old vote
      LOG(log_dg_) << "Drop vote " << vote->getHash() << ". Vote period " << vote->getPeriod()
                   << " too old. current_pbft_period " << current_pbft_period;
      continue;
    }

    votes.push_back(std::move(vote));
  }

  LOG(log_nf_) << "Received " << next_votes_count << " (processed " << votes.size() << " ) sync votes from peer "
               << packet_data.from_node_id_ << " node current round " << current_pbft_round << ", peer pbft round "
               << votes_bundle_pbft_round;

  // Previous round next votes
  if (votes_bundle_votes_type == PbftVoteTypes::next_vote) {
    const auto [pbft_round, pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();
    const auto two_t_plus_one = vote_mgr_->getPbftTwoTPlusOne(pbft_period - 1);
    // Check if we did not move to the next period/round in the meantime
    if (votes_bundle_pbft_period == pbft_period && votes_bundle_pbft_round == (pbft_round - 1) &&
        two_t_plus_one.has_value()) {
      // Update our previous round next vote bundles...
      next_votes_mgr_->updateWithSyncedVotes(votes, *two_t_plus_one);
    }
  }

  onNewPbftVotesBundle(std::move(votes), false, packet_data.from_node_id_);
}

void VotesSyncPacketHandler::broadcastPreviousRoundNextVotesBundle(bool rebroadcast) {
  auto next_votes_bundle = next_votes_mgr_->getNextVotes();
  if (next_votes_bundle.empty()) {
    LOG(log_er_) << "There are empty next votes for previous PBFT round";
    return;
  }

  const auto pbft_current_round = pbft_mgr_->getPbftRound();

  for (auto const &peer : peers_state_->getAllPeers()) {
    // Nodes may vote at different values at previous round, so need less or equal
    if (!peer.second->syncing_ && peer.second->pbft_round_ <= pbft_current_round) {
      std::vector<std::shared_ptr<Vote>> send_next_votes_bundle;
      for (const auto &v : next_votes_bundle) {
        if (rebroadcast || !peer.second->isVoteKnown(v->getHash())) {
          send_next_votes_bundle.push_back(v);
        }
      }
      sendPbftVotesBundle(peer.second, std::move(send_next_votes_bundle));
    }
  }
}

void VotesSyncPacketHandler::onNewPbftVotesBundle(std::vector<std::shared_ptr<Vote>> &&votes, bool rebroadcast,
                                                  const std::optional<dev::p2p::NodeID> &exclude_node) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      continue;
    }

    if (exclude_node.has_value() && *exclude_node == peer.first) {
      continue;
    }

    std::vector<std::shared_ptr<Vote>> peer_votes;
    for (const auto &vote : votes) {
      if (!rebroadcast && peer.second->isVoteKnown(vote->getHash())) {
        continue;
      }

      peer_votes.push_back(vote);
    }

    sendPbftVotesBundle(peer.second, std::move(peer_votes));
  }
}

}  // namespace taraxa::network::tarcap
