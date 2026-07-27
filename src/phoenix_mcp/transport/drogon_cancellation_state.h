#ifndef PHOENIX_MCP_TRANSPORT_DROGON_CANCELLATION_STATE_H_
#define PHOENIX_MCP_TRANSPORT_DROGON_CANCELLATION_STATE_H_

#include <folly/CancellationToken.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace phoenix_mcp::server::drogon_internal {

class DrogonCancellationState {
 public:
  std::pair<folly::CancellationToken, bool> register_connection(
      const std::string& connection_id);
  folly::CancellationToken token_for(const std::string& connection_id);
  void close(const std::string& connection_id);
  bool contains(const std::string& connection_id) const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<folly::CancellationSource>>
      sources_;
};

}  // namespace phoenix_mcp::server::drogon_internal

#endif  // PHOENIX_MCP_TRANSPORT_DROGON_CANCELLATION_STATE_H_
