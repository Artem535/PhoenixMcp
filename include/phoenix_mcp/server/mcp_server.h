#ifndef PHOENIX_MCP_SERVER_MCP_SERVER_H_
#define PHOENIX_MCP_SERVER_MCP_SERVER_H_

#include <memory>
#include <utility>

#include "phoenix_mcp/server/mcp_request_handler.h"
#include "phoenix_mcp/transport/i_transport.h"

namespace phoenix_mcp::server {

class McpServer {
 public:
  McpServer(std::unique_ptr<ITransport> transport,
            std::unique_ptr<McpRequestHandler> handler)
      : transport_(std::move(transport)), handler_(std::move(handler)) {}

  int run() {
    return transport_->run([this](ITransport::RequestEnvelope request) {
      return handler_->handle_json_async(std::move(request));
    });
  }

 private:
  std::unique_ptr<ITransport> transport_;
  std::unique_ptr<McpRequestHandler> handler_;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_SERVER_MCP_SERVER_H_
