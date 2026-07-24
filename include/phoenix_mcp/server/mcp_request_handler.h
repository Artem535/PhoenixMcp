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

class SessionManager;

/// @brief Dispatches transport requests to per-connection `ServerSession`s.
///
/// Backed by a `SessionManager` keyed on `RequestEnvelope::connection_id`, so
/// every distinct connection gets its own session instead of one shared,
/// process-lifetime session. Transports that only ever serve a single
/// connection per process (stdio) simply never set `connection_id`; that's
/// treated as one shared default connection rather than "no session" — the
/// same `SessionManager`-backed code path handles it, there is no bypass.
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
  std::optional<ITransport::ResponseEnvelope> handle_json(
      const ITransport::RequestEnvelope& request);
  folly::coro::Task<std::optional<ITransport::ResponseEnvelope>>
  handle_json_async(ITransport::RequestEnvelope request);

 private:
  std::unique_ptr<SessionManager> session_manager_;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_SERVER_MCP_REQUEST_HANDLER_H_
