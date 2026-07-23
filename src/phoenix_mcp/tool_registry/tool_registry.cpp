//
// Created by artem.d on 09.11.2025.
//

#include "tool_registry.h"

#include <folly/coro/BlockingWait.h>

#include <ranges>

namespace phoenix_mcp::tool {
msg::types::CallToolResult ToolRegistry::call_tool(const std::string& name,
                                                   const rfl::Generic& params) {
  return folly::coro::blockingWait(call_tool_async(name, params));
}

folly::coro::Task<msg::types::CallToolResult> ToolRegistry::call_tool_async(
    const std::string& name, const rfl::Generic& params,
    folly::CancellationToken cancel_token) {
  // Find the tool in the registry
  const auto& tool = tools_.find(name);
  if (tool == tools_.end()) {
    throw std::runtime_error("ToolRegistry::call_tool| Tool not found: " +
                             name);
  }

  // Call the tool
  co_return co_await tool->second(params, cancel_token);
}

std::vector<msg::types::Tool> ToolRegistry::get_tool_list() {
  // Reserve size
  const auto values = tool_descriptions_ | std::views::values;
  std::vector<msg::types::Tool> tools{values.begin(), values.end()};
  return std::move(tools);
}

}  // namespace phoenix_mcp::tool
