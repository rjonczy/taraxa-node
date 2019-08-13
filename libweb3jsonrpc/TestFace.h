/**
 * This file is generated by jsonrpcstub, DO NOT CHANGE IT MANUALLY!
 */

#ifndef JSONRPC_CPP_STUB_DEV_RPC_TESTFACE_H_
#define JSONRPC_CPP_STUB_DEV_RPC_TESTFACE_H_

#include "ModularServer.h"

namespace dev {
namespace rpc {
class TestFace : public ServerInterface<TestFace> {
 public:
  TestFace() {
    this->bindAndAddMethod(
        jsonrpc::Procedure("insert_dag_block", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::insert_dag_blockI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("insert_stamped_dag_block",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::insert_stamped_dag_blockI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_block", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_dag_blockI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_block_children",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_dag_block_childrenI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_block_siblings",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_dag_block_siblingsI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_block_tips", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_dag_block_tipsI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_block_pivot_chain",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_dag_block_pivot_chainI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_block_subtree", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_dag_block_subtreeI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_block_epfriend",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_dag_block_epfriendI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("send_coin_transaction", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::send_coin_transactionI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("create_test_coin_transactions",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::create_test_coin_transactionsI);
    this->bindAndAddMethod(jsonrpc::Procedure("get_num_proposed_blocks",
                                              jsonrpc::PARAMS_BY_POSITION,
                                              jsonrpc::JSON_OBJECT, NULL),
                           &TestFace::get_num_proposed_blocksI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("send_pbft_schedule_block",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::send_pbft_schedule_blockI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_account_address", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_account_addressI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_account_balance", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_account_balanceI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_peer_count", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_peer_countI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_all_peers", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_all_peersI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("node_stop", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::node_stopI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("node_reset", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::node_resetI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("node_start", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::node_startI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("should_speak", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::should_speakI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("place_vote", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::place_voteI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_votes", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_votesI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("draw_graph", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::draw_graphI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_transaction_count", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_transaction_countI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_size", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_dag_sizeI);
  }
  inline virtual void insert_dag_blockI(const Json::Value &request,
                                        Json::Value &response) {
    response = this->insert_dag_block(request[0u]);
  }
  inline virtual void insert_stamped_dag_blockI(const Json::Value &request,
                                                Json::Value &response) {
    response = this->insert_stamped_dag_block(request[0u]);
  }
  inline virtual void get_dag_blockI(const Json::Value &request,
                                     Json::Value &response) {
    response = this->get_dag_block(request[0u]);
  }
  inline virtual void get_dag_block_childrenI(const Json::Value &request,
                                              Json::Value &response) {
    response = this->get_dag_block_children(request[0u]);
  }
  inline virtual void get_dag_block_siblingsI(const Json::Value &request,
                                              Json::Value &response) {
    response = this->get_dag_block_siblings(request[0u]);
  }
  inline virtual void get_dag_block_tipsI(const Json::Value &request,
                                          Json::Value &response) {
    response = this->get_dag_block_tips(request[0u]);
  }
  inline virtual void get_dag_block_pivot_chainI(const Json::Value &request,
                                                 Json::Value &response) {
    response = this->get_dag_block_pivot_chain(request[0u]);
  }
  inline virtual void get_dag_block_subtreeI(const Json::Value &request,
                                             Json::Value &response) {
    response = this->get_dag_block_subtree(request[0u]);
  }
  inline virtual void get_dag_block_epfriendI(const Json::Value &request,
                                              Json::Value &response) {
    response = this->get_dag_block_epfriend(request[0u]);
  }
  inline virtual void send_coin_transactionI(const Json::Value &request,
                                             Json::Value &response) {
    response = this->send_coin_transaction(request[0u]);
  }
  inline virtual void create_test_coin_transactionsI(const Json::Value &request,
                                                     Json::Value &response) {
    response = this->create_test_coin_transactions(request[0u]);
  }
  inline virtual void get_num_proposed_blocksI(const Json::Value &request,
                                               Json::Value &response) {
    response = this->get_num_proposed_blocks();
  }
  inline virtual void send_pbft_schedule_blockI(const Json::Value &request,
                                                Json::Value &response) {
    response = this->send_pbft_schedule_block(request[0u]);
  }
  inline virtual void get_account_addressI(const Json::Value &request,
                                           Json::Value &response) {
    response = this->get_account_address();
  }
  inline virtual void get_account_balanceI(const Json::Value &request,
                                           Json::Value &response) {
    response = this->get_account_balance(request[0u]);
  }
  inline virtual void get_peer_countI(const Json::Value &request,
                                      Json::Value &response) {
    response = this->get_peer_count();
  }
  inline virtual void get_all_peersI(const Json::Value &request,
                                     Json::Value &response) {
    response = this->get_all_peers();
  }
  inline virtual void node_stopI(const Json::Value &request,
                                 Json::Value &response) {
    response = this->node_stop();
  }
  inline virtual void node_resetI(const Json::Value &request,
                                  Json::Value &response) {
    response = this->node_reset();
  }
  inline virtual void node_startI(const Json::Value &request,
                                  Json::Value &response) {
    response = this->node_start();
  }
  inline virtual void should_speakI(const Json::Value &request,
                                    Json::Value &response) {
    response = this->should_speak(request[0u]);
  }
  inline virtual void place_voteI(const Json::Value &request,
                                  Json::Value &response) {
    response = this->place_vote(request[0u]);
  }
  inline virtual void get_votesI(const Json::Value &request,
                                 Json::Value &response) {
    response = this->get_votes(request[0u]);
  }
  inline virtual void draw_graphI(const Json::Value &request,
                                  Json::Value &response) {
    response = this->draw_graph(request[0u]);
  }
  inline virtual void get_transaction_countI(const Json::Value &request,
                                             Json::Value &response) {
    response = this->get_transaction_count(request[0u]);
  }
  inline virtual void get_executed_trx_countI(const Json::Value &request,
                                              Json::Value &response) {
    response = this->get_executed_trx_count(request[0u]);
  }
  inline virtual void get_executed_blk_countI(const Json::Value &request,
                                              Json::Value &response) {
    response = this->get_executed_blk_count(request[0u]);
  }
  inline virtual void get_dag_sizeI(const Json::Value &request,
                                    Json::Value &response) {
    response = this->get_dag_size(request[0u]);
  }
  virtual Json::Value insert_dag_block(const Json::Value &param1) = 0;
  virtual Json::Value insert_stamped_dag_block(const Json::Value &param1) = 0;
  virtual Json::Value get_dag_block(const Json::Value &param1) = 0;
  virtual Json::Value get_dag_block_children(const Json::Value &param1) = 0;
  virtual Json::Value get_dag_block_siblings(const Json::Value &param1) = 0;
  virtual Json::Value get_dag_block_tips(const Json::Value &param1) = 0;
  virtual Json::Value get_dag_block_pivot_chain(const Json::Value &param1) = 0;
  virtual Json::Value get_dag_block_subtree(const Json::Value &param1) = 0;
  virtual Json::Value get_dag_block_epfriend(const Json::Value &param1) = 0;
  virtual Json::Value send_coin_transaction(const Json::Value &param1) = 0;
  virtual Json::Value create_test_coin_transactions(
      const Json::Value &param1) = 0;
  virtual Json::Value get_num_proposed_blocks() = 0;
  virtual Json::Value send_pbft_schedule_block(const Json::Value &param1) = 0;
  virtual Json::Value get_account_address() = 0;
  virtual Json::Value get_account_balance(const Json::Value &param1) = 0;
  virtual Json::Value get_peer_count() = 0;
  virtual Json::Value get_all_peers() = 0;
  virtual Json::Value node_stop() = 0;
  virtual Json::Value node_reset() = 0;
  virtual Json::Value node_start() = 0;
  virtual Json::Value should_speak(const Json::Value &param1) = 0;
  virtual Json::Value place_vote(const Json::Value &param1) = 0;
  virtual Json::Value get_votes(const Json::Value &param1) = 0;
  virtual Json::Value draw_graph(const Json::Value &param1) = 0;
  virtual Json::Value get_transaction_count(const Json::Value &param1) = 0;
  virtual Json::Value get_executed_trx_count(const Json::Value &param1) = 0;
  virtual Json::Value get_executed_blk_count(const Json::Value &param1) = 0;

  virtual Json::Value get_dag_size(const Json::Value &param1) = 0;
};
}  // namespace rpc
}  // namespace dev
#endif  // JSONRPC_CPP_STUB_DEV_RPC_TESTFACE_H_
