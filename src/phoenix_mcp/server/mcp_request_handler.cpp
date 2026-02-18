//
// Created by artem.d on 18.02.2026.
//

#include "mcp_request_handler.h"

#include <utility>

namespace pxm::server {

McpRequestHandler::McpRequestHandler(
    msg::types::ServerCapabilities server_capabilities,
    msg::types::Implementation server_info,
    std::string instruction,
    std::unique_ptr<tool::ToolRegistry> tool_registry)
    : session_(std::make_unique<McpSession>(std::move(server_capabilities),
                                            std::move(server_info),
                                            std::move(instruction),
                                            std::move(tool_registry))) {
}

std::optional<std::string> McpRequestHandler::handle_json(
    const std::string& request_json) {
  const auto result = session_->handle_input(request_json);
  if (!result.has_value()) {
    return std::nullopt;
  }

  return rfl::json::write(result.value());
}

} // namespace pxm::server
