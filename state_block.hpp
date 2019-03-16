/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:37 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-14 17:30:09
 */

#ifndef STATE_BLOCK_HPP
#define STATE_BLOCK_HPP

#include <iostream>
#include <string>
#include <cryptopp/aes.h>
#include <cryptopp/blake2.h>
#include <condition_variable>
#include <thread>
#include <deque>
#include <boost/thread.hpp>
#include "util.hpp"
#include "libdevcore/Log.h"

namespace taraxa{
using std::string;
class DagManager;
// Block definition

class StateBlock{
public:
	StateBlock() = default;
	StateBlock(StateBlock && blk);
	StateBlock(StateBlock const & blk) = default;
	StateBlock(blk_hash_t pivot, 
				vec_tip_t tips, 
				vec_trx_t trxs,
				sig_t signature, 
				blk_hash_t hash,
				name_t publisher
				);
	StateBlock(stream &strm);
	StateBlock(const string &json);
	friend std::ostream & operator<<(std::ostream &str, StateBlock &u){
		str<<"	pivot		= "<< u.pivot_ << std::endl;
		str<<"	tips		= ";
		for (auto const &t: u.tips_)
			str<< t <<" ";
		str<<std::endl;
		str<<"	trxs		= ";
		for (auto const &t: u.trxs_)
			str<< t <<" ";
		str<<std::endl;
		str<<"	signature	= "<< u.signature_<<std::endl;
		str<<"	hash		= "<< u.hash_<<std::endl;
		str<<"  publisher   = "<< u.publisher_<<std::endl;

		return str;
	}
	bool operator== (StateBlock const & other) const;
	StateBlock & operator=(StateBlock && block);
	StateBlock & operator=(StateBlock const & block) = default;
	blk_hash_t getPivot() const;
	vec_tip_t getTips() const ; 
	vec_trx_t getTrxs() const;
	sig_t getSignature() const;
	blk_hash_t getHash() const;
	name_t getPublisher() const;
	std::string getJsonStr() const;
	bool isValid() const;
	bool serialize (stream & strm) const;
	bool deserialize (stream & strm);

private:

	constexpr static unsigned nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	
	blk_hash_t pivot_ = "";
	vec_tip_t tips_; 
	vec_trx_t trxs_; // transactions
	sig_t signature_ = "";
	blk_hash_t hash_ = "";
	name_t publisher_ = ""; //block creater
};

/**
 * Thread safe
 */

class BlockQueue{
public: 
	BlockQueue(size_t capacity, unsigned verify_threads);
	~BlockQueue();
	void pushUnverifiedBlock(StateBlock const & block); // add to unverified queue
	StateBlock getVerifiedBlock(); // get one verified block and pop
	void start();
	void stop();

private:
	using uLock = std::unique_lock<std::mutex>;
	using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
	using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

	void verifyBlock();
	bool stopped_ = true;
	size_t capacity_ = 2048;
	size_t num_verifiers_ = 1;
	mutable boost::shared_mutex shared_mutex_; // shared mutex to check seen_blocks ...
	mutable std::mutex mutex_; // mutex

	// seen blks
	std::unordered_set<blk_hash_t> seen_blocks_;

	std::vector<std::thread> verifiers_;
	mutable std::mutex mutex_for_unverified_qu_;
	mutable std::mutex mutex_for_verified_qu_;

	std::condition_variable cond_for_unverified_qu_;
	std::condition_variable cond_for_verified_qu_;

	std::deque<StateBlock> unverified_qu_;
	std::deque<StateBlock> verified_qu_;
	dev::Logger logger_ { dev::createLogger(dev::Verbosity::VerbosityInfo, "bq")};
	dev::Logger logger_debug_ { dev::createLogger(dev::Verbosity::VerbosityDebug, "bq")};
};

} // namespace taraxa
#endif