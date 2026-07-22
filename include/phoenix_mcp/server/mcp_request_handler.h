#ifndef PHOENIX_MCP_SERVER_MCP_REQUEST_HANDLER_H_
#define PHOENIX_MCP_SERVER_MCP_REQUEST_HANDLER_H_

#include <folly/coro/Task.h>

#include <memory>
#include <optional>
#include <string>

#include "phoenix_mcp/protocol/message_types.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "phoenix_mcp/transport/i_transport.h"

namespace phoenix_mcp::server {

class McpSession;

class McpRequestHandler {
 public:
  McpRequestHandler(msg::types::ServerCapabilities server_capabilities,
                    msg::types::Implementation server_info,
                    std::string instruction,
                    std::unique_ptr<tool::ToolRegistry> tool_registry);
  ~McpRequestHandler();

  std::optional<std::string> handle_json(const std::string& request_json);
  folly::coro::Task<std::optional<std::string>> handle_json_async(
      std::string request_json);
  std::optional<std::string> handle_json(
      const ITransport::RequestEnvelope& request);
  folly::coro::Task<std::optional<std::string>> handle_json_async(
      ITransport::RequestEnvelope request);

 private:
  std::unique_ptr<McpSession> session_;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_SERVER_MCP_REQUEST_HANDLER_H_
