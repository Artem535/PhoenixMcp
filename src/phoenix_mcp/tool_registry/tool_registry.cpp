//
// Created by artem.d on 09.11.2025.
//

#include "tool_registry.h"

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