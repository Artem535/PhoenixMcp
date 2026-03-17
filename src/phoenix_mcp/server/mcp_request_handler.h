//
// Created by artem.d on 18.02.2026.
//

#pragma once

#include <folly/coro/Task.h>

#include <memory>
#include <optional>
#include <string>

#include "mcp_session.h"
#include "../transport/i_transport.h"

namespace pxm::server {

/// Thin adapter around McpSession that exposes string-in/string-out API.
class McpRequestHandler {
public:
  McpRequestHandler(msg::types::ServerCapabilities server_capabilities,
                    msg::types::Implementation server_info,
                    std::string instruction,
                    std::unique_ptr<tool::ToolRegistry> tool_registry);

  /// Returns JSON-RPC response body if request requires response.
  std::optional<std::string> handle_json(const std::string& request_json);
  folly::coro::Task<std::optional<std::string>> handle_json_async(
      std::string request_json);
  std::optional<std::string> handle_json(const ITransport::RequestEnvelope& request);
  folly::coro::Task<std::optional<std::string>> handle_json_async(
      ITransport::RequestEnvelope request);

private:
  std::unique_ptr<McpSession> session_;
};

} // namespace pxm::server
