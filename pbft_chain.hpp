#ifndef TARAXA_NODE_PBFT_CHAIN_HPP
#define TARAXA_NODE_PBFT_CHAIN_HPP

#include <libdevcore/Common.h>
#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcore/db.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>

#include <iostream>
#include <string>
#include <vector>

#include "types.hpp"
#include "util.hpp"

/**
 * In pbft_chain, two kinds of blocks:
 * 1. PivotBlock: determine DAG block in pivot chain
 * 2. ScheduleBlock: determine sequential/concurrent set
 */
namespace taraxa {
using boost::property_tree::ptree;
class DbStorage;
//enum PbftBlockTypes {
//  pbft_block_none_type = -1,
//  pivot_block_type = 0,
//  schedule_block_type
//};

enum PbftVoteTypes {
  propose_vote_type = 0,
  soft_vote_type,
  cert_vote_type,
  next_vote_type
};

struct TrxSchedule {
 public:
  enum class TrxStatus : uint8_t { sequential = 1, parallel };

  TrxSchedule() = default;
  TrxSchedule(
      vec_blk_t const& blks,
      std::vector<std::vector<std::pair<trx_hash_t, uint>>> const& modes)
      : dag_blks_order(blks), trxs_mode(modes) {}
  // Construct from RLP
  TrxSchedule(bytes const& rlpData);
  ~TrxSchedule() {}

  // order of DAG blocks (in hash)
  vec_blk_t dag_blks_order;
  // It is multiple array of transactions
  // TODO: optimize trx_mode type
  std::vector<std::vector<std::pair<trx_hash_t, uint>>> trxs_mode;
  bytes rlp() const;
//  std::string getJsonStr() const;
  // TODO: need unit test for serialize/deserialize, setPtree, setSchedule
  bool serialize(stream& strm) const;
  bool deserialize(stream& strm);
  void setPtree(ptree& tree) const;
  void setSchedule(ptree const& tree);
  bool operator==(TrxSchedule const& other) const {
    return rlp() == other.rlp();
  }
  std::string getStr() const;
};
std::ostream& operator<<(std::ostream& strm, TrxSchedule const& trx_sche);

class FullNode;
class Vote;
/*
class PivotBlock {
 public:
  PivotBlock() = default;
  PivotBlock(taraxa::stream& strm);
  PivotBlock(std::string const& json);

  PivotBlock(blk_hash_t const& prev_pivot_hash,
             blk_hash_t const& prev_block_hash,
             blk_hash_t const& dag_block_hash, uint64_t period,
             addr_t beneficiary)
      : prev_pivot_hash_(prev_pivot_hash),
        prev_block_hash_(prev_block_hash),
        dag_block_hash_(dag_block_hash),
        period_(period),
        beneficiary_(beneficiary) {}
  ~PivotBlock() {}

  blk_hash_t getPrevPivotBlockHash() const;
  blk_hash_t getPrevBlockHash() const;
  blk_hash_t getDagBlockHash() const;
  uint64_t getPeriod() const;
  addr_t getBeneficiary() const;

  void setJsonTree(ptree& tree) const;
  void setBlockByJson(ptree const& doc);

  bool serialize(stream& strm) const;
  bool deserialize(stream& strm);
  void streamRLP(dev::RLPStream& strm) const;

  friend std::ostream& operator<<(std::ostream& strm,
                                  PivotBlock const& pivot_block) {
    strm << "[Pivot Block]" << std::endl;
    strm << "  previous pivot hash: " << pivot_block.prev_pivot_hash_.hex()
         << std::endl;
    strm << "  previous result hash: " << pivot_block.prev_block_hash_.hex()
         << std::endl;
    strm << "  dag hash: " << pivot_block.dag_block_hash_.hex() << std::endl;
    strm << "  period: " << pivot_block.period_ << std::endl;
    strm << "  beneficiary: " << pivot_block.beneficiary_.hex() << std::endl;
    return strm;
  }

 private:
  blk_hash_t prev_pivot_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_;
  uint64_t period_;
  addr_t beneficiary_;
};

class ScheduleBlock {
 public:
  ScheduleBlock() = default;
  ScheduleBlock(blk_hash_t const& prev_block_hash, TrxSchedule const& schedule)
      : prev_block_hash_(prev_block_hash), schedule_(schedule) {}
  ScheduleBlock(taraxa::stream& strm);
  ~ScheduleBlock() {}
  void streamRLP(dev::RLPStream& strm) const;

  std::string getJsonStr() const;
  Json::Value getJson() const;
  TrxSchedule getSchedule() const;
  blk_hash_t getPrevBlockHash() const;

  void setJsonTree(ptree& tree) const;
  void setBlockByJson(ptree const& json);

  bool serialize(stream& strm) const;
  bool deserialize(stream& strm);

  friend std::ostream;

 private:
  blk_hash_t prev_block_hash_;
  TrxSchedule schedule_;
};
std::ostream& operator<<(std::ostream& strm, ScheduleBlock const& sche_blk);
*/
class PbftBlock {
 public:
  PbftBlock() = default;
//  PbftBlock(uint64_t height) : height_(height) {}
  PbftBlock(blk_hash_t const& block_hash, uint64_t height)
      : block_hash_(block_hash), height_(height) {} // For unit test
  PbftBlock(blk_hash_t const& prev_blk_hash,
            blk_hash_t const& dag_blk_hash_as_pivot,
            TrxSchedule const& schedule,  uint64_t period, uint64_t height,
            uint64_t timestamp, addr_t const& beneficiary)
      : prev_block_hash_(prev_blk_hash),
        dag_block_hash_as_pivot_(dag_blk_hash_as_pivot),
        schedule_(schedule),
        period_(period),
        height_(height),
        timestamp_(timestamp),
        beneficiary_(beneficiary) {}
//  PbftBlock(PivotBlock const& pivot_block, uint64_t height)
//      : pivot_block_(pivot_block),
//        block_type_(pivot_block_type),
//        height_(height) {}
//  PbftBlock(ScheduleBlock const& schedule_block, uint64_t height)
//      : schedule_block_(schedule_block),
//        block_type_(schedule_block_type),
//        height_(height) {}
  PbftBlock(dev::RLP const& r);
  PbftBlock(bytes const& b);

  PbftBlock(std::string const& str);
  ~PbftBlock() {}

  bool serialize(stream& strm) const;
  bool deserialize(stream& strm);
  std::string getJsonStr(bool with_signature = true) const;
  addr_t getAuthor() const;
  void streamRLP(dev::RLPStream& strm) const;
  bytes rlp() const;
  void serializeRLP(dev::RLPStream& s) const;

  blk_hash_t getBlockHash() const { return block_hash_; }
  blk_hash_t getPrevBlockHash() const { return prev_block_hash_; }
  blk_hash_t getPivotDagBlockHash() const { return dag_block_hash_as_pivot_; }
  TrxSchedule getSchedule() const { return schedule_; }
  uint64_t getPeriod() const { return period_; }
  uint64_t getHeight() const { return height_; }
  uint64_t getTimestamp() const { return timestamp_; }
  addr_t getBeneficiary() const { return beneficiary_; }

  void setBlockHash();
  void setPrevBlockHash(blk_hash_t const& prev_block_hash);
  void setDagBlockHashAsPivot(blk_hash_t const& dag_block_hash);
  void setSchedule(TrxSchedule const& schedule);
//  void setBlockType(PbftBlockTypes block_type);
//  void setPivotBlock(PivotBlock const& pivot_block);
//  void setScheduleBlock(ScheduleBlock const& schedule_block);
  void setPeriod(uint64_t const period);
  void setHeight(uint64_t const height);
  void setTimestamp(uint64_t const timestamp);
  void setBeneficiary(addr_t const& beneficiary);
  void setSignature(sig_t const& signature);

 private:
  blk_hash_t block_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_as_pivot_;
  TrxSchedule schedule_;

//  PbftBlockTypes block_type_ = pbft_block_none_type;
//  PivotBlock pivot_block_;
//  ScheduleBlock schedule_block_;
  uint64_t period_;
  uint64_t height_;
  uint64_t timestamp_;
  addr_t beneficiary_;
  sig_t signature_;
};
std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk);

struct PbftBlockCert {
  PbftBlockCert(PbftBlock const& pbft_blk, std::vector<Vote> const& cert_votes);
  PbftBlockCert(bytes const& all_rlp);
  PbftBlockCert(PbftBlock const& pbft_blk, bytes const& cert_votes_rlp);

  PbftBlock pbft_blk;
  std::vector<Vote> cert_votes;
  bytes rlp() const;
};
std::ostream& operator<<(std::ostream& strm, PbftBlockCert const& b);

class PbftChain {
 public:
  PbftChain(std::string const& dag_genesis_hash);
  virtual ~PbftChain() = default;

  void setFullNode(std::shared_ptr<FullNode> node);
  void setPbftGenesis(std::string const& pbft_genesis_str);

  void cleanupUnverifiedPbftBlocks(taraxa::PbftBlock const& pbft_block);

  uint64_t getPbftChainSize() const { return size_; }
  uint64_t getPbftChainPeriod() const { return period_; }
  //  uint64_t getPbftChainPeriod() const { return period_; }
  blk_hash_t getGenesisHash() const { return genesis_hash_; }
  blk_hash_t getLastPbftBlockHash() const { return last_pbft_block_hash_; }
//  blk_hash_t getLastPbftPivotHash() const { return last_pbft_pivot_hash_; }
//  PbftBlockTypes getNextPbftBlockType() const { return next_pbft_block_type_; }

  PbftBlock getPbftBlockInChain(blk_hash_t const& pbft_block_hash);
  std::pair<PbftBlock, bool> getUnverifiedPbftBlock(
      blk_hash_t const& pbft_block_hash);
  std::vector<PbftBlock> getPbftBlocks(size_t height, size_t count) const;
  std::vector<std::string> getPbftBlocksStr(size_t height, size_t count,
                                            bool hash) const;
  std::string getGenesisStr() const;
  std::string getJsonStr() const;
//  dev::Logger& getLoggerErr() { return log_err_; }
  std::pair<blk_hash_t, bool> getDagBlockHash(uint64_t dag_block_height) const;
  std::pair<uint64_t, bool> getDagBlockHeight(
      blk_hash_t const& dag_block_hash) const;
  uint64_t getDagBlockMaxHeight() const;

  void setLastPbftBlockHash(blk_hash_t const& new_pbft_block);
//  void setNextPbftBlockType(PbftBlockTypes next_block_type);  // Test only

  bool findPbftBlockInChain(blk_hash_t const& pbft_block_hash) const;
  bool findUnverifiedPbftBlock(blk_hash_t const& pbft_block_hash) const;
  bool findPbftBlockInSyncedSet(blk_hash_t const& pbft_block_hash) const;

  bool pushPbftBlockIntoChain(taraxa::PbftBlock const& pbft_block);
  bool pushPbftBlock(taraxa::PbftBlock const& pbft_block);
//  bool pushPbftPivotBlock(taraxa::PbftBlock const& pbft_block);
//  bool pushPbftScheduleBlock(taraxa::PbftBlock const& pbft_block);
  void pushUnverifiedPbftBlock(taraxa::PbftBlock const& pbft_block);
  uint64_t pushDagBlockHash(blk_hash_t const& dag_block_hash);

  bool checkPbftBlockValidationFromSyncing(
      taraxa::PbftBlock const& pbft_block) const;
  bool checkPbftBlockValidation(taraxa::PbftBlock const& pbft_block) const;

  bool pbftSyncedQueueEmpty() const;
  PbftBlockCert pbftSyncedQueueFront() const;
  PbftBlockCert pbftSyncedQueueBack() const;
  void pbftSyncedQueuePopFront();
  void setSyncedPbftBlockIntoQueue(PbftBlockCert const& pbft_block_and_votes);
  void clearSyncedPbftBlocks();
  size_t pbftSyncedQueueSize() const;

 private:
  void pbftSyncedSetInsert_(blk_hash_t const& pbft_block_hash);
  void pbftSyncedSetErase_();
  void insertPbftBlockIndex_(blk_hash_t const& pbft_block_hash);
  void insertUnverifiedPbftBlockIntoParentMap_(
      blk_hash_t const& prev_block_hash, blk_hash_t const& block_hash);

  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  mutable boost::shared_mutex sync_access_;
  mutable boost::shared_mutex unverified_access_;

  blk_hash_t genesis_hash_; // pbft chain head hash
  uint64_t size_;
  uint64_t period_;
//  PbftBlockTypes next_pbft_block_type_;
  blk_hash_t last_pbft_block_hash_;
//  blk_hash_t last_pbft_pivot_hash_;

  blk_hash_t dag_genesis_hash_;
  uint64_t max_dag_blocks_height_ = 0;

  std::weak_ptr<FullNode> node_;
  std::shared_ptr<DbStorage> db_ = nullptr;

  // <prev block hash, vector<PBFT proposed blocks waiting for vote>>
  std::unordered_map<blk_hash_t, std::vector<blk_hash_t>>
      unverified_blocks_map_;
  std::unordered_map<blk_hash_t, PbftBlock> unverified_blocks_;

  // syncing pbft blocks from peers
  std::deque<PbftBlockCert> pbft_synced_queue_;
  std::unordered_set<blk_hash_t> pbft_synced_set_;

  mutable dev::Logger log_sil_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "PBFT_CHAIN")};
  mutable dev::Logger log_err_{
      dev::createLogger(dev::Verbosity::VerbosityError, "PBFT_CHAIN")};
  mutable dev::Logger log_war_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "PBFT_CHAIN")};
  mutable dev::Logger log_inf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFT_CHAIN")};
  mutable dev::Logger log_deb_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "PBFT_CHAIN")};
  mutable dev::Logger log_tra_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "PBFT_CHAIN")};
};
std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain);

}  // namespace taraxa
#endif
