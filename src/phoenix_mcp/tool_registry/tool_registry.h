//
// Created by artem.d on 09.11.2025.
//
#pragma once

#include <map>
#include <string>
#include <functional>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <rfl/Generic.hpp>

#include "abstract_tool_params.h"
#include "../types/msg_types.hpp"

namespace pxm::tool {
/// @brief Function type for internal tool handlers that work with generic parameters
using ToolHandlerInternal = std::function<msg::types::CallToolResult(
    const rfl::Generic& params)>;

/// @brief Template function type for tool handlers with specific parameter types
/// @tparam Params The parameter struct type for this tool
template <typename Params>
using ToolHandler = std::function<msg::types::CallToolResult(
    const Params& params)>;

/// @brief Registry for managing available tools in the MCP server
/// 
/// This class handles registration of tools with their schemas and handlers,
/// allowing the server to dynamically expose tools to clients.
class ToolRegistry {
public:
  /// @brief Register a new tool with the registry
  /// 
  /// @tparam Params The parameter struct type for this tool
  /// @param name Unique name for the tool
  /// @param description Human-readable description of what the tool does
  /// @param handler Function that implements the tool's behavior
  template <typename Params>
  void registerTool(const std::string& name, const std::string& description,
                    const ToolHandler<Params>& handler);

  msg::types::CallToolResult call_tool(const std::string& name,
                                       const rfl::Generic& params);

  std::vector<msg::types::Tool> get_tool_list();

private:
  /// Map of tool names to their internal handlers
  std::map<std::string, ToolHandlerInternal> tools_;

  /// Map of tool names to their metadata descriptions
  std::map<std::string, msg::types::Tool> tool_descriptions_;

  /// @brief Extract the actual schema definition from a JSON schema with references
  /// @param obj JSON schema that may contain $ref references
  /// @return nlohmann::json The flattened schema without references
  static nlohmann::json get_flat_scheme(const nlohmann::json& obj);
};

template <typename Params>
void ToolRegistry::registerTool(const std::string& name,
                                const std::string& description,
                                const ToolHandler<Params>& handler) {
  // Generate JSON schema for the parameter type
  const auto schema_str = rfl::json::to_schema<Params>();
  const auto obj = nlohmann::json::parse(schema_str);
  const auto flat_scheme = get_flat_scheme(obj);

  // Extract properties from the schema
  std::map<std::string, rfl::Generic> properties;
  for (const auto& [key, value] : flat_scheme["properties"].items()) {
    properties[key] = rfl::Generic(value.dump());
  }

  // Extract required fields if they exist
  std::optional<std::vector<std::string> > required;
  if (flat_scheme.contains("required")) {
    required = flat_scheme["required"].template get<std::vector<
      std::string> >();
  }

  // Create input schema structure
  const msg::types::InputSchema input_schema = {
      .properties = properties,
      .required = required,
  };

  // Create tool description structure
  const msg::types::Tool tool = {
      .name = name,
      .description = description,
      .input_schema = input_schema,
  };

  // Log the tool specification for debugging
  spdlog::debug(
      "ToolRegistry::registerTool| Create the tool with this specification: {}",
      rfl::json::write(tool));

  // Store tool description and wrap handler for internal use
  tool_descriptions_[name] = tool;
  tools_[name] = [handler](const rfl::Generic& generic_params) {
    // Convert generic parameters to the specific type
    Params params = rfl::from_generic<Params>(generic_params).value();
    // Call the actual handler
    return handler(params);
  };

  spdlog::debug("ToolRegistry::registerTool| Tool {} registered", name);
}
}