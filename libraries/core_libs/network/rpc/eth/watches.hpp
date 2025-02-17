#pragma once

#include <chrono>
#include <queue>

#include "LogFilter.hpp"
#include "common/global_const.hpp"
#include "data.hpp"

namespace taraxa::net::rpc::eth {

using time_point = std::chrono::system_clock::time_point;

// TODO taraxa simple exception
DEV_SIMPLE_EXCEPTION(WatchLimitExceeded);

enum WatchType {
  new_blocks,
  new_transactions,
  logs,

  // do not touch
  COUNT,
};

struct WatchGroupConfig {
  uint64_t max_watches = 0;
  std::chrono::seconds idle_timeout{5 * 60};
};

using WatchesConfig = std::array<WatchGroupConfig, WatchType::COUNT>;

using WatchID = uint64_t;

GLOBAL_CONST(WatchType, watch_id_type_mask_bits);

struct placeholder_t {};
template <WatchType type_, typename InputType_, typename OutputType_ = placeholder_t, typename Params = placeholder_t>
class WatchGroup {
 public:
  static constexpr auto type = type_;
  using InputType = InputType_;
  using OutputType = std::conditional_t<std::is_same_v<OutputType_, placeholder_t>, InputType, OutputType_>;
  using time_point = std::chrono::high_resolution_clock::time_point;
  using Updater = std::function<void(Params const&,     //
                                     InputType const&,  //
                                     std::function<void(OutputType const&)> const& /*do_update*/)>;

  struct Watch {
    Params params;
    time_point last_touched{};
    std::vector<OutputType> updates{};
    mutable util::DefaultConstructCopyableMovable<std::shared_mutex> mu{};
  };

 private:
  WatchGroupConfig cfg_;
  Updater updater_;
  mutable std::unordered_map<WatchID, Watch> watches_;
  mutable std::shared_mutex watches_mu_;
  mutable WatchID watch_id_seq_ = 0;

 public:
  explicit WatchGroup(WatchesConfig const& cfg = {}, Updater&& updater = {})
      : cfg_(cfg[type]), updater_(std::move(updater)) {
    assert(cfg_.idle_timeout.count() != 0);
    if constexpr (std::is_same_v<InputType, OutputType>) {
      if (!updater_) {
        updater_ = [](auto const&, auto const& input, auto const& do_update) { do_update(input); };
      }
    } else {
      assert(updater_);
    }
  }

  WatchID install_watch(Params&& params = {}) const {
    std::unique_lock l(watches_mu_);
    if (cfg_.max_watches && watches_.size() == cfg_.max_watches) {
      throw WatchLimitExceeded();
    }
    auto id = ((++watch_id_seq_) << watch_id_type_mask_bits()) + type;
    watches_.insert_or_assign(id, Watch{std::move(params), std::chrono::high_resolution_clock::now()});
    return id;
  }

  bool uninstall_watch(WatchID watch_id) const {
    std::unique_lock l(watches_mu_);
    return watches_.erase(watch_id);
  }

  void uninstall_stale_watches() const {
    std::unique_lock l(watches_mu_);
    bool did_uninstall = false;
    for (auto& [id, watch] : watches_) {
      if (cfg_.idle_timeout <=
          duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - watch.last_touched)) {
        watches_.erase(id);
        did_uninstall = true;
      }
    }
    if (auto num_buckets = watches_.bucket_count(); did_uninstall && (1 << 10) < num_buckets) {
      if (size_t desired_num_buckets = 1 << uint(ceil(log2(watches_.size()))); desired_num_buckets != num_buckets) {
        watches_.rehash(desired_num_buckets);
      }
    }
  }

  std::optional<Params> get_watch_params(WatchID watch_id) const {
    std::shared_lock l(watches_mu_);
    if (auto entry = watches_.find(watch_id); entry != watches_.end()) {
      return entry->second.params;
    }
    return {};
  }

  void process_update(InputType const& obj_in) const {
    std::shared_lock l(watches_mu_);
    for (auto& entry : watches_) {
      auto& watch = entry.second;
      updater_(watch.params, obj_in, [&](auto const& obj_out) {
        std::unique_lock l(watch.mu.val);
        watch.updates.push_back(obj_out);
      });
    }
  }

  auto poll(WatchID watch_id) const {
    std::vector<OutputType> ret;
    std::shared_lock l(watches_mu_);
    if (auto entry = watches_.find(watch_id); entry != watches_.end()) {
      auto& watch = entry->second;
      std::unique_lock l1(watch.mu.val);
      swap(ret, watch.updates);
      watch.last_touched = std::chrono::high_resolution_clock::now();
    }
    return ret;
  }
};

class Watches {
 public:
  WatchesConfig const cfg_;

  WatchGroup<WatchType::new_blocks, h256> const new_blocks_{cfg_};
  WatchGroup<WatchType::new_transactions, h256> const new_transactions_{cfg_};
  WatchGroup<WatchType::logs,  //
             std::pair<ExtendedTransactionLocation const&, TransactionReceipt const&>, LocalisedLogEntry,
             LogFilter> const logs_{
      cfg_,
      [](auto const& log_filter, auto const& input, auto const& do_update) {
        auto const& [trx_loc, receipt] = input;
        log_filter.match_one(trx_loc, receipt, do_update);
      },
  };

  template <typename Visitor>
  auto visit(WatchType type, Visitor&& visitor) {
    switch (type) {
      case WatchType::new_blocks:
        return visitor(&new_blocks_);
      case WatchType::new_transactions:
        return visitor(&new_transactions_);
      case WatchType::logs:
        return visitor(&logs_);
      default:
        assert(false);
    }
  }

 private:
  std::condition_variable watch_cleaner_wait_cv_;
  std::thread watch_cleaner_;
  std::atomic<bool> destructor_called_ = false;

 public:
  Watches(WatchesConfig const& _cfg);
  ~Watches();

  Watches(const Watches&) = delete;
  Watches(Watches&&) = delete;
  Watches& operator=(const Watches&) = delete;
  Watches& operator=(Watches&&) = delete;

  template <typename Visitor>
  auto visit_by_id(WatchID watch_id, Visitor&& visitor) {
    if (auto type = WatchType(watch_id & ((1 << watch_id_type_mask_bits()) - 1)); type < COUNT) {
      return visit(type, std::forward<Visitor>(visitor));
    }
    return visitor(decltype (&new_blocks_)(nullptr));
  }
};

}  // namespace taraxa::net::rpc::eth