#ifndef PHOENIX_MCP_TRANSPORT_DROGON_TRANSPORT_H_
#define PHOENIX_MCP_TRANSPORT_DROGON_TRANSPORT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "phoenix_mcp/core/runtime.h"
#include "phoenix_mcp/transport/i_transport.h"

namespace phoenix_mcp::server {

namespace drogon_internal {
class DrogonCancellationState;
}

namespace streamable_http_internal {
class StreamableHttpSessionStore;
}

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
  std::shared_ptr<ServerMessageSink> message_sink() override;

 private:
  Config cfg_;
  std::shared_ptr<runtime::Runtime> runtime_;
  std::shared_ptr<drogon_internal::DrogonCancellationState> cancellation_state_;
  std::shared_ptr<streamable_http_internal::StreamableHttpSessionStore>
      session_store_;
  std::shared_ptr<ServerMessageSink> message_sink_;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_TRANSPORT_DROGON_TRANSPORT_H_
