#include "dag/sortition_params_manager.hpp"

#include "pbft/pbft_block.hpp"

#define NULL_BLOCK_HASH blk_hash_t(0)

namespace taraxa {

SortitionParamsChange::SortitionParamsChange(uint64_t period, uint16_t efficiency, const VrfParams& vrf)
    : period(period), vrf_params(vrf), interval_efficiency(efficiency) {}

bytes SortitionParamsChange::rlp() const {
  dev::RLPStream s;
  s.appendList(4);
  s << vrf_params.threshold_range;
  s << vrf_params.threshold_upper;
  s << period;
  s << interval_efficiency;

  return s.invalidate();
}

SortitionParamsChange SortitionParamsChange::from_rlp(const dev::RLP& rlp) {
  SortitionParamsChange p;

  p.vrf_params.threshold_range = rlp[0].toInt<uint16_t>();
  p.vrf_params.threshold_upper = rlp[1].toInt<uint16_t>();
  p.period = rlp[2].toInt<uint64_t>();
  p.interval_efficiency = rlp[3].toInt<uint16_t>();

  return p;
}

SortitionParamsManager::SortitionParamsManager(const addr_t& node_addr, SortitionConfig sort_conf,
                                               std::shared_ptr<DbStorage> db)
    : config_(std::move(sort_conf)), db_(std::move(db)) {
  LOG_OBJECTS_CREATE("SORT_MGR");
  // load cache values from db
  params_changes_ = db_->getLastSortitionParams(config_.changes_count_for_average);
  if (params_changes_.empty()) {
    // if no changes in db save default vrf params
    auto batch = db_->createWriteBatch();
    SortitionParamsChange pc{0, config_.targetEfficiency(), config_.vrf};
    db_->saveSortitionParamsChange(0, pc, batch);
    db_->commitWriteBatch(batch);
    params_changes_.push_back(pc);
  } else {
    // restore VRF params from last change
    config_.vrf = params_changes_.back().vrf_params;
  }

  auto period = params_changes_.back().period + 1;
  ignored_efficiency_counter_ = 0;
  for (auto data = db_->getPeriodDataRaw(period); data.size() > 0; period++) {
    SyncBlock sync_block(data);
    if (sync_block.pbft_blk->getPivotDagBlockHash() != NULL_BLOCK_HASH) {
      if (ignored_efficiency_counter_ >= config_.computation_interval - config_.changing_interval) {
        uint16_t dag_efficiency = calculateDagEfficiency(sync_block);
        dag_efficiencies_.push_back(dag_efficiency);
      } else {
        ignored_efficiency_counter_++;
      }
    }
  }
}

SortitionParams SortitionParamsManager::getSortitionParams(std::optional<uint64_t> period) const {
  if (!period || (config_.changing_interval == 0)) {
    return config_;
  }
  SortitionParams p = config_;
  auto change = db_->getParamsChangeForPeriod(period.value());
  if (change.has_value()) {
    p.vrf = change->vrf_params;
  }

  return p;
}

uint16_t SortitionParamsManager::calculateDagEfficiency(const SyncBlock& block) const {
  // calculate efficiency only for current block because it is not worth to check if transaction was finalized before
  size_t total_transactions_count = 0;
  std::unordered_set<trx_hash_t> unique_transactions;
  for (const auto& dag_block : block.dag_blocks) {
    if (dag_block.getDifficulty() == config_.vdf.difficulty_stale) {
      continue;
    }
    const auto& trxs = dag_block.getTrxs();
    unique_transactions.insert(trxs.begin(), trxs.end());
    total_transactions_count += trxs.size();
  }

  if (total_transactions_count == 0) return 100 * kOnePercent;

  return unique_transactions.size() * 100 * kOnePercent / total_transactions_count;
}

uint16_t SortitionParamsManager::averageDagEfficiency() {
  return std::accumulate(dag_efficiencies_.begin(), dag_efficiencies_.end(), 0) / dag_efficiencies_.size();
}

void SortitionParamsManager::cleanup() {
  dag_efficiencies_.clear();
  while (params_changes_.size() > config_.changes_count_for_average) {
    params_changes_.pop_front();
  }
}

void SortitionParamsManager::pbftBlockPushed(const SyncBlock& block, DbStorage::Batch& batch,
                                             size_t non_empty_pbft_chain_size) {
  if (config_.changing_interval == 0) {
    return;
  }
  if (ignored_efficiency_counter_ >= config_.computation_interval - config_.changing_interval) {
    uint16_t dag_efficiency = calculateDagEfficiency(block);
    dag_efficiencies_.push_back(dag_efficiency);
    const auto& period = block.pbft_blk->getPeriod();
    LOG(log_dg_) << period << " pbftBlockPushed, efficiency: " << dag_efficiency / 100. << "%";

    if (non_empty_pbft_chain_size % config_.changing_interval == 0) {
      const auto params_change = calculateChange(period);
      params_changes_.push_back(params_change);
      db_->saveSortitionParamsChange(period, params_change, batch);
      cleanup();
    }
  } else {
    ignored_efficiency_counter_++;
  }
}

int32_t efficiencyToChange(uint16_t efficiency, uint16_t goal_efficiency) {
  uint16_t deviation = std::abs(efficiency - goal_efficiency) * 100 / goal_efficiency;
  // If goal is 50 % return 1% change for 40%-60%, 2% change for 30%-70% and 5% change for over that
  if (deviation < 20) {
    return UINT16_MAX / 100;
  }
  if (deviation < 40) {
    return UINT16_MAX / 50;
  }
  return UINT16_MAX / 20;
}

int32_t SortitionParamsManager::getNewUpperRange(uint16_t efficiency) const {
  if (efficiency >= config_.dag_efficiency_targets.first && efficiency <= config_.dag_efficiency_targets.second) {
    return params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper;
  }

  // efficiencies_to_uppper_range provide mapping from efficiency to VRF upper threshold, params_changes contain
  // efficiency for previous setting so mapping is done efficiency of i relates to VRF upper threshold of (i + 1)
  std::map<uint16_t, uint32_t> efficiencies_to_uppper_range;
  const uint16_t goal_efficiency = (config_.dag_efficiency_targets.first + config_.dag_efficiency_targets.second) / 2;
  for (uint32_t i = 1; i < params_changes_.size(); i++) {
    efficiencies_to_uppper_range[params_changes_[i].interval_efficiency] =
        params_changes_[i - 1].vrf_params.threshold_upper;
  }
  if (params_changes_.size() > 1) {
    efficiencies_to_uppper_range[efficiency] = params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper;
  }

  // Check if all last params are below goal_efficiency
  if ((efficiencies_to_uppper_range.empty() || efficiencies_to_uppper_range.rbegin()->first < goal_efficiency) &&
      efficiency < goal_efficiency) {
    // If last params are under goal_efficiency and we are still under goal_efficiency, decrease upper
    // limit
    return ((int32_t)params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) -
           efficiencyToChange(efficiency, goal_efficiency);
  }

  // Check if all last params are over goal_efficiency
  if ((efficiencies_to_uppper_range.empty() || efficiencies_to_uppper_range.begin()->first >= goal_efficiency) &&
      efficiency >= goal_efficiency) {
    // If last params are over goal_efficiency and we are still over goal_efficiency, increase upper
    // limit
    return ((int32_t)params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) +
           efficiencyToChange(efficiency, goal_efficiency);
  }

  // If efficiency is less than goal_efficiency find the efficiency over goal_efficiency closest to goal_efficiency
  if (efficiency < goal_efficiency) {
    for (const auto& eff : efficiencies_to_uppper_range) {
      if (eff.first >= goal_efficiency) {
        if (eff.second < params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) {
          // Return average between last range and the one over goal_efficiency and closest to goal_efficiency
          return (eff.second + params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) / 2;
        } else {
          return ((int32_t)params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) -
                 efficiencyToChange(efficiency, goal_efficiency);
        }
      }
    }
  }

  // If efficiency is over goal_efficiency find the efficiency below goal_efficiency closest to goal_efficiency
  if (efficiency >= goal_efficiency) {
    for (auto eff = efficiencies_to_uppper_range.rbegin(); eff != efficiencies_to_uppper_range.rend(); ++eff) {
      if (eff->first < goal_efficiency) {
        if (eff->second > params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) {
          // Return average between last range and the one below goal_efficiency and closest to goal_efficiency
          return (eff->second + params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) / 2;
        } else {
          return ((int32_t)params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) +
                 efficiencyToChange(efficiency, goal_efficiency);
        }
      }
    }
  }

  // It should not be possible to reach here
  assert(false);
}

SortitionParamsChange SortitionParamsManager::calculateChange(uint64_t period) {
  const auto average_dag_efficiency = averageDagEfficiency();

  int32_t new_upper_range = getNewUpperRange(average_dag_efficiency);
  if (new_upper_range < VrfParams::kThresholdUpperMinValue) {
    new_upper_range = VrfParams::kThresholdUpperMinValue;
  } else if (new_upper_range > UINT16_MAX) {
    new_upper_range = UINT16_MAX;
  }

  config_.vrf.threshold_upper = new_upper_range;
  LOG(log_si_) << "Average interval efficiency: " << average_dag_efficiency / 100. << "% . Changing VRF params on "
               << period << " period to (" << config_.vrf.threshold_upper << ")";

  return SortitionParamsChange{period, average_dag_efficiency, config_.vrf};
}

}  // namespace taraxa