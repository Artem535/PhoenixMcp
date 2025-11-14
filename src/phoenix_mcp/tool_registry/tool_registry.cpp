//
// Created by artem.d on 09.11.2025.
//

#include "tool_registry.h"

#include <ranges>

namespace pxm::tool {
msg::types::CallToolResult ToolRegistry::call_tool(const std::string& name,
                                                   const rfl::Generic& params) {
  // Find the tool in the registry
  const auto& tool = tools_.find(name);
  if (tool == tools_.end()) {
    throw std::runtime_error(
        "ToolRegistry::call_tool| Tool not found: " + name);
  }

  // Call the tool
  const auto result = tool->second(params);
  return result;
}

std::vector<pxm::msg::types::Tool> ToolRegistry::get_tool_list() {
  // Reserve size
  const auto values = tool_descriptions_ | std::views::values;
  std::vector<msg::types::Tool> tools{values.begin(), values.end()};
  return std::move(tools);
}

}