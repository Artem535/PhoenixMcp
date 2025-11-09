//
// Created by artem.d on 09.11.2025.
//

#include "tool_registry.h"

namespace pxm::tool {
rfl::Generic ToolRegistry::callTool(const std::string& name,
                                    const rfl::Generic& params) {
  // Find the tool in the registry
  const auto& tool = tools_.find(name);
  if (tool == tools_.end()) {
    throw std::runtime_error("ToolRegistry::callTool| Tool not found: " + name);
  }

  // Call the tool
  const auto results = tool->second(params);
  return results;
}

std::vector<pxm::msg::types::Tool> ToolRegistry::get_tool_list() {
  // Reserve size
  const auto values = tool_descriptions_ | std::views::values;
  std::vector<msg::types::Tool> tools{values.begin(), values.end()};
  return std::move(tools);
}

nlohmann::json pxm::tool::ToolRegistry::get_flat_scheme(
    const nlohmann::json& obj) {

  if (!obj.contains("$ref")) {
    return obj;
  }

  const std::string ref = obj["$ref"];
  const auto last_position = ref.find_last_of('/');
  const auto class_name = ref.substr(last_position + 1);
  return obj["$defs"][class_name];
}

}