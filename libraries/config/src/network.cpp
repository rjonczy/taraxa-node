#include "config/network.hpp"

#include "config/config_utils.hpp"

namespace taraxa {

void dec_json(const Json::Value &json, PrometheusConfig &config) {
  config.listen_port = getConfigData(json, {"listen_port"}).asUInt();
  config.polling_interval_ms = getConfigData(json, {"polling_interval_ms"}).asUInt();
}

void ConnectionConfig::validate() const {
  if (!http_port && !ws_port) {
    throw ConfigException("Either http_port or ws_port post must be specified for connection config");
  }

  // Max enabled number of threads for processing rpc requests
  constexpr uint16_t MAX_RPC_THREADS_NUM = 10;
  if (threads_num <= 0 || threads_num > MAX_RPC_THREADS_NUM) {
    throw ConfigException(std::string("threads_num must be in range (0, ") + std::to_string(MAX_RPC_THREADS_NUM) + "]");
  }
}

void dec_json(const Json::Value &json, ConnectionConfig &config) {
  // http port
  if (auto http_port = getConfigData(json, {"http_port"}, true); !http_port.isNull()) {
    config.http_port = http_port.asUInt();
  }

  // websocket port
  if (auto ws_port = getConfigData(json, {"ws_port"}, true); !ws_port.isNull()) {
    config.ws_port = ws_port.asUInt();
  }

  // number of threads processing rpc calls
  if (auto threads_num = getConfigData(json, {"threads_num"}, true); !threads_num.isNull()) {
    config.threads_num = threads_num.asUInt();
  }
}

void NetworkConfig::validate() const {
  if (rpc) {
    rpc->validate();
  }
  if (graphql) {
    graphql->validate();
  }
  if (sync_level_size == 0) {
    throw ConfigException(std::string("network.sync_level_size cannot be 0"));
  }

  // Max enabled number of threads for processing rpc requests
  constexpr uint16_t MAX_PACKETS_PROCESSING_THREADS_NUM = 30;
  if (packets_processing_threads < 3 || packets_processing_threads > MAX_PACKETS_PROCESSING_THREADS_NUM) {
    throw ConfigException(std::string("network.packets_processing_threads must be in range [3, ") +
                          std::to_string(MAX_PACKETS_PROCESSING_THREADS_NUM) + "]");
  }

  if (transaction_interval_ms == 0) {
    throw ConfigException(std::string("network.transaction_interval_ms must be greater than zero"));
  }

  // TODO validate that the boot node list doesn't contain self (although it's not critical)
  for (const auto &node : boot_nodes) {
    if (node.ip.empty()) {
      throw ConfigException(std::string("Boot node ip is empty:") + node.ip + ":" + std::to_string(node.port));
    }
    if (node.port == 0) {
      throw ConfigException(std::string("Boot node port invalid: ") + std::to_string(node.port));
    }
  }
}

NodeConfig dec_json(const Json::Value &json) {
  NodeConfig node;
  node.id = getConfigDataAsString(json, {"id"});
  node.ip = getConfigDataAsString(json, {"ip"});
  node.port = getConfigDataAsUInt(json, {"port"});
  return node;
}

void dec_json(const Json::Value &json, NetworkConfig &network) {
  network.listen_ip = getConfigDataAsString(json, {"listen_ip"});
  network.public_ip = getConfigDataAsString(json, {"public_ip"}, true);
  network.listen_port = getConfigDataAsUInt(json, {"listen_port"});
  network.transaction_interval_ms = getConfigDataAsUInt(json, {"transaction_interval_ms"});
  network.ideal_peer_count = getConfigDataAsUInt(json, {"ideal_peer_count"});
  network.max_peer_count = getConfigDataAsUInt(json, {"max_peer_count"});
  network.sync_level_size = getConfigDataAsUInt(json, {"sync_level_size"});
  network.collect_packets_stats = getConfigDataAsBoolean(json, {"collect_packets_stats"});
  network.packets_processing_threads = getConfigDataAsUInt(json, {"packets_processing_threads"});
  network.peer_blacklist_timeout =
      getConfigDataAsUInt(json, {"peer_blacklist_timeout"}, true, NetworkConfig::kBlacklistTimeoutDefaultInSeconds);
  network.disable_peer_blacklist = getConfigDataAsBoolean(json, {"disable_peer_blacklist"}, true, false);
  network.deep_syncing_threshold =
      getConfigDataAsUInt(json, {"deep_syncing_threshold"}, true, network.deep_syncing_threshold);
  network.vote_accepting_periods =
      getConfigDataAsUInt(json, {"vote_accepting_periods"}, true, network.vote_accepting_periods);
  network.vote_accepting_rounds =
      getConfigDataAsUInt(json, {"vote_accepting_rounds"}, true, network.vote_accepting_rounds);
  network.vote_accepting_steps =
      getConfigDataAsUInt(json, {"vote_accepting_steps"}, true, network.vote_accepting_steps);
  for (auto &item : json["boot_nodes"]) {
    network.boot_nodes.push_back(dec_json(item));
  }
  auto listen_ip = boost::asio::ip::address::from_string(network.listen_ip);
  // Rpc config
  if (auto rpc_json = getConfigData(json, {"rpc"}, true); !rpc_json.isNull()) {
    network.rpc.emplace();
    // ip address
    network.rpc->address = listen_ip;

    dec_json(rpc_json, *network.rpc);
  }

  // GraphQL config
  if (auto graphql_json = getConfigData(json, {"graphql"}, true); !graphql_json.isNull()) {
    network.graphql.emplace();
    // ip address
    network.graphql->address = listen_ip;

    dec_json(graphql_json, *network.graphql);
  }

  if (auto prometheus_json = getConfigData(json, {"prometheus"}, true); !prometheus_json.isNull()) {
    network.prometheus.emplace();
    // ip address
    network.prometheus->address = network.listen_ip;

    dec_json(prometheus_json, *network.prometheus);
  }
}

}  // namespace taraxa