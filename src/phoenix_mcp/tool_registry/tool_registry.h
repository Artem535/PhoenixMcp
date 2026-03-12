//
// Created by artem.d on 09.11.2025.
//
#pragma once

#include <folly/coro/Task.h>
#include <folly/coro/ViaIfAsync.h>

#include <memory>
#include <map>
#include <string>
#include <functional>

#include <spdlog/spdlog.h>
#include <rfl/json.hpp>
#include <rfl/Generic.hpp>

#include "utils.hpp"
#include "../runtime/runtime.h"
#include "../types/msg_types.hpp"

namespace pxm::tool {
/// @brief Function type for internal tool handlers that work with generic parameters
using AsyncToolHandlerInternal = std::function<
    folly::coro::Task<msg::types::CallToolResult>(const rfl::Generic& params)>;

/// @brief Template function type for tool handlers with specific parameter types
/// @tparam InputParams The parameter struct type for this tool
template <typename InputParams>
using ToolHandler = std::function<msg::types::CallToolResult(
    const InputParams& params)>;

template <typename InputParams, typename OutputParams>
using ToolHandlerWithOutput = std::function<OutputParams(const InputParams& params)>;

template <typename InputParams>
using AsyncToolHandler = std::function<
    folly::coro::Task<msg::types::CallToolResult>(const InputParams& params)>;

template <typename InputParams, typename OutputParams>
using AsyncToolHandlerWithOutput = std::function<
    folly::coro::Task<OutputParams>(const InputParams& params)>;

/// @brief Registry for managing available tools in the MCP server
/// 
/// This class handles registration of tools with their schemas and handlers,
/// allowing the server to dynamically expose tools to clients.
class ToolRegistry {
public:
  enum class ExecutionPolicy {
    Inline,
    CpuBound,
    IoBound,
  };

  explicit ToolRegistry(std::shared_ptr<runtime::Runtime> runtime =
                            runtime::make_default_runtime())
      : runtime_(std::move(runtime)) {
  }

  /// @brief Register a new tool with the registry
  /// 
  /// @tparam InputParams The parameter struct type for this tool
  /// @param name Unique name for the tool
  /// @param description Human-readable description of what the tool does
  /// @param handler Function that implements the tool's behavior
  template <typename InputParams>
  void register_tool(const std::string& name, const std::string& description,
                     const ToolHandler<InputParams>& handler) {
    register_async_tool<InputParams>(
        name, description,
        [handler](const InputParams& params)
            -> folly::coro::Task<msg::types::CallToolResult> {
          co_return handler(params);
        });
  }

  template <typename InputParams, typename OutputParams>
  void register_tool(const std::string& name, const std::string& description,
                     const ToolHandlerWithOutput<InputParams, OutputParams>& handler) {
    register_async_tool<InputParams, OutputParams>(
        name, description,
        [handler](const InputParams& params) -> folly::coro::Task<OutputParams> {
          co_return handler(params);
        });
  }

  template <typename InputParams>
  void register_async_tool(const std::string& name,
                           const std::string& description,
                           const AsyncToolHandler<InputParams>& handler,
                           const ExecutionPolicy policy =
                               ExecutionPolicy::CpuBound) {
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
    auto runtime = runtime_;
    tools_[name] = [handler, runtime, policy](const rfl::Generic& generic_params)
        -> folly::coro::Task<msg::types::CallToolResult> {
      // Convert generic parameters to the specific type
      InputParams params = rfl::from_generic<InputParams>(generic_params).value();
      // Call the actual handler
      auto task = handler(params);
      switch (policy) {
        case ExecutionPolicy::Inline:
          co_return co_await std::move(task);
        case ExecutionPolicy::CpuBound:
          co_return co_await folly::coro::co_viaIfAsync(
              runtime->cpu_executor(), std::move(task));
        case ExecutionPolicy::IoBound:
          co_return co_await folly::coro::co_viaIfAsync(
              runtime->io_executor(), std::move(task));
      }

      co_return co_await std::move(task);
    };

    spdlog::debug("ToolRegistry::register_tool| Tool {} registered", name);
  }

  template <typename InputParams, typename OutputParams>
  void register_async_tool(
      const std::string& name, const std::string& description,
      const AsyncToolHandlerWithOutput<InputParams, OutputParams>& handler,
      const ExecutionPolicy policy = ExecutionPolicy::CpuBound) {
    register_async_tool<InputParams>(
        name, description,
        [handler](const InputParams& params)
            -> folly::coro::Task<msg::types::CallToolResult> {
          const auto output = co_await handler(params);
          const auto output_str = rfl::json::write(output);
          co_return utils::make_text_result(output_str);
        },
        policy);
  }

  msg::types::CallToolResult call_tool(const std::string& name,
                                       const rfl::Generic& params);
  folly::coro::Task<msg::types::CallToolResult> call_tool_async(
      const std::string& name, const rfl::Generic& params);

  std::vector<msg::types::Tool> get_tool_list();

private:
  /// Map of tool names to their internal handlers
  std::map<std::string, AsyncToolHandlerInternal> tools_;

  /// Map of tool names to their metadata descriptions
  std::map<std::string, msg::types::Tool> tool_descriptions_;

  std::shared_ptr<runtime::Runtime> runtime_;

};

}
