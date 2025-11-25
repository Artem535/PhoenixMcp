//
// Created by artem.d on 09.11.2025.
//
#pragma once

#include <map>
#include <string>
#include <functional>

#include <spdlog/spdlog.h>
#include <rfl/json.hpp>
#include <rfl/Generic.hpp>

#include "utils.hpp"
#include "../types/msg_types.hpp"

namespace pxm::tool {
/// @brief Function type for internal tool handlers that work with generic parameters
using ToolHandlerInternal = std::function<msg::types::CallToolResult(
    const rfl::Generic& params)>;

/// @brief Template function type for tool handlers with specific parameter types
/// @tparam InputParams The parameter struct type for this tool
template <typename InputParams>
using ToolHandler = std::function<msg::types::CallToolResult(
    const InputParams& params)>;

template <typename InputParams, typename OutputParams>
using ToolHandlerWithOutput = std::function<OutputParams(const InputParams& params)>;

/// @brief Registry for managing available tools in the MCP server
/// 
/// This class handles registration of tools with their schemas and handlers,
/// allowing the server to dynamically expose tools to clients.
class ToolRegistry {
public:
  /// @brief Register a new tool with the registry
  /// 
  /// @tparam InputParams The parameter struct type for this tool
  /// @param name Unique name for the tool
  /// @param description Human-readable description of what the tool does
  /// @param handler Function that implements the tool's behavior
  template <typename InputParams>
  void register_tool(const std::string& name, const std::string& description,
                     const ToolHandler<InputParams>& handler) {
    // Generate JSON schema for the parameter type
    const auto schema_str = rfl::json::to_schema<InputParams>();
    auto schema = rfl::json::read<msg::types::ToolInputSchema>(schema_str).
        value();

    std::string ref = schema.ref.value();
    const size_t suffix_position = ref.find_last_of('/');
    ref = ref.substr(suffix_position + 1);

    // Create tool description structure
    const msg::types::Tool tool = {
      .name = name,
      .description = description,
      .input_schema = schema.defs.value()[ref],
  };

    // Log the tool specification for debugging
    spdlog::debug(
        "ToolRegistry::register_tool| Create the tool with this specification: {}",
        rfl::json::write(tool));

    // Store tool description and wrap handler for internal use
    tool_descriptions_[name] = tool;
    tools_[name] = [handler](const rfl::Generic& generic_params) {
      // Convert generic parameters to the specific type
      InputParams params = rfl::from_generic<InputParams>(generic_params).value();
      // Call the actual handler
      return handler(params);
    };

    spdlog::debug("ToolRegistry::register_tool| Tool {} registered", name);
  }

  template <typename InputParams, typename OutputParams>
  void register_tool(const std::string& name, const std::string& description,
                     const ToolHandlerWithOutput<InputParams, OutputParams>& handler) {
    // Generate JSON schema for the parameter type
    const auto schema_str = rfl::json::to_schema<InputParams>();
    auto schema = rfl::json::read<msg::types::ToolInputSchema>(schema_str).
        value();

    std::string ref = schema.ref.value();
    const size_t suffix_position = ref.find_last_of('/');
    ref = ref.substr(suffix_position + 1);

    // Create tool description structure
    const msg::types::Tool tool = {
      .name = name,
      .description = description,
      .input_schema = schema.defs.value()[ref],
  };

    // Log the tool specification for debugging
    spdlog::debug(
        "ToolRegistry::register_tool| Create the tool with this specification: {}",
        rfl::json::write(tool));

    // Store tool description and wrap handler for internal use
    tool_descriptions_[name] = tool;
    tools_[name] = [handler](const rfl::Generic& generic_params) {
      // Convert generic parameters to the specific type
      InputParams params = rfl::from_generic<InputParams>(generic_params).value();
      // Call the actual handler
      const auto output = handler(params);
      const auto output_str = rfl::json::write(output);
      return utils::make_text_result(output_str);
    };

    spdlog::debug("ToolRegistry::register_tool| Tool {} registered", name);
  }

  msg::types::CallToolResult call_tool(const std::string& name,
                                       const rfl::Generic& params);

  std::vector<msg::types::Tool> get_tool_list();

private:
  /// Map of tool names to their internal handlers
  std::map<std::string, ToolHandlerInternal> tools_;

  /// Map of tool names to their metadata descriptions
  std::map<std::string, msg::types::Tool> tool_descriptions_;

};

}