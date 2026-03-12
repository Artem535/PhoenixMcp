//
// Created by artem.d on 18.02.2026.
//

#include "mcp_request_handler.h"

#include <folly/coro/BlockingWait.h>

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
    const std::string &request_json) {
    return folly::coro::blockingWait(handle_json_async(request_json));
  }

  folly::coro::Task<std::optional<std::string>>
  McpRequestHandler::handle_json_async(std::string request_json) {
    const auto result = co_await session_->handle_input_async(
        std::move(request_json));
    if (!result.has_value()) {
      co_return std::nullopt;
    }

    co_return rfl::json::write(result.value());
  }
} // namespace pxm::server
