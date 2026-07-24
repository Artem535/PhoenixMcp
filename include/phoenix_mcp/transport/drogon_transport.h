#ifndef PHOENIX_MCP_TRANSPORT_DROGON_TRANSPORT_H_
#define PHOENIX_MCP_TRANSPORT_DROGON_TRANSPORT_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <folly/CancellationToken.h>

#include "phoenix_mcp/core/runtime.h"
#include "phoenix_mcp/transport/i_transport.h"

namespace phoenix_mcp::server {

class DrogonTransport final : public ITransport {
 public:
  struct Config {
    std::string bind_address = "0.0.0.0";
    uint16_t port = 8080;
    std::string endpoint = "/mcp";
    std::string health_endpoint = "/health";
    int concurrency = 1;
  };

  DrogonTransport();
  explicit DrogonTransport(Config cfg,
                           std::shared_ptr<runtime::Runtime> runtime =
                               runtime::make_default_runtime());

  int run(Handler on_message) override;

 private:
  Config cfg_;
  std::shared_ptr<runtime::Runtime> runtime_;

  // One CancellationSource per connection (keyed by the same peer-address
  // id every request on that connection already uses), triggered when the
  // underlying TCP connection closes. An uncancelled source is safe to
  // reuse across every request on the same connection, so this only needs
  // to be created once per connection (on its first request) and cleaned
  // up when the connection's close callback fires.
  std::mutex cancel_sources_mutex_;
  std::unordered_map<std::string, folly::CancellationSource> cancel_sources_;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_TRANSPORT_DROGON_TRANSPORT_H_
