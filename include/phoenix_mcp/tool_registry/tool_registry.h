#ifndef PHOENIX_MCP_TOOL_REGISTRY_TOOL_REGISTRY_H_
#define PHOENIX_MCP_TOOL_REGISTRY_TOOL_REGISTRY_H_

#include <folly/CancellationToken.h>
#include <folly/coro/Task.h>
#include <folly/coro/ViaIfAsync.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <map>
#include <memory>
#include <rfl/Generic.hpp>
#include <rfl/json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "phoenix_mcp/core/runtime.h"
#include "phoenix_mcp/protocol/message_types.h"
#include "phoenix_mcp/tool_registry/utils.h"

namespace phoenix_mcp::tool {

using AsyncToolHandlerInternal =
    std::function<folly::coro::Task<msg::types::CallToolResult>(
        const rfl::Generic& params, const folly::CancellationToken& cancel_token)>;

template <typename InputParams>
using ToolHandler =
    std::function<msg::types::CallToolResult(const InputParams& params)>;

template <typename InputParams, typename OutputParams>
using ToolHandlerWithOutput =
    std::function<OutputParams(const InputParams& params)>;

template <typename InputParams>
using AsyncToolHandler =
    std::function<folly::coro::Task<msg::types::CallToolResult>(
        const InputParams& params)>;

template <typename InputParams, typename OutputParams>
using AsyncToolHandlerWithOutput =
    std::function<folly::coro::Task<OutputParams>(const InputParams& params)>;

// Handler variant for tools that want to cooperate with cancellation (e.g.
// long-running operations): checks `cancel_token.isCancellationRequested()`
// itself and returns accordingly. Tools that don't need this can keep using
// the plain `AsyncToolHandler`/`ToolHandler` above.
template <typename InputParams>
using AsyncCancellableToolHandler =
    std::function<folly::coro::Task<msg::types::CallToolResult>(
        const InputParams& params, const folly::CancellationToken& cancel_token)>;

class ToolRegistry {
 public:
  enum class ExecutionPolicy { Inline, CpuBound, IoBound };

  explicit ToolRegistry(std::shared_ptr<runtime::Runtime> runtime =
                            runtime::make_default_runtime())
      : runtime_(std::move(runtime)) {}

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
  void register_tool(
      const std::string& name, const std::string& description,
      const ToolHandlerWithOutput<InputParams, OutputParams>& handler) {
    register_async_tool<InputParams, OutputParams>(
        name, description,
        [handler](const InputParams& params)
            -> folly::coro::Task<OutputParams> { co_return handler(params); });
  }

  template <typename InputParams>
  void register_async_tool(const std::string& name,
                           const std::string& description,
                           const AsyncToolHandler<InputParams>& handler,
                           ExecutionPolicy policy = ExecutionPolicy::CpuBound) {
    register_cancellable_tool<InputParams>(
        name, description,
        [handler](const InputParams& params,
                  const folly::CancellationToken&)
            -> folly::coro::Task<msg::types::CallToolResult> {
          co_return co_await handler(params);
        },
        policy);
  }

  template <typename InputParams>
  void register_cancellable_tool(
      const std::string& name, const std::string& description,
      const AsyncCancellableToolHandler<InputParams>& handler,
      ExecutionPolicy policy = ExecutionPolicy::CpuBound) {
    const auto schema_string = rfl::json::to_schema<InputParams>();
    auto schema =
        rfl::json::read<msg::types::ToolInputSchema>(schema_string).value();
    std::string ref = schema.ref.value();
    ref = ref.substr(ref.find_last_of('/') + 1);
    const msg::types::Tool tool{
        .name = name,
        .description = description,
        .input_schema = schema.defs.value()[ref],
    };
    tool_descriptions_[name] = tool;
    auto runtime = runtime_;
    tools_[name] = [handler, runtime, policy](
                       const rfl::Generic& generic_params,
                       const folly::CancellationToken& cancel_token)
        -> folly::coro::Task<msg::types::CallToolResult> {
      InputParams params =
          rfl::from_generic<InputParams>(generic_params).value();
      auto task = handler(params, cancel_token);
      switch (policy) {
        case ExecutionPolicy::Inline:
          co_return co_await std::move(task);
        case ExecutionPolicy::CpuBound:
          co_return co_await folly::coro::co_viaIfAsync(runtime->cpu_executor(),
                                                        std::move(task));
        case ExecutionPolicy::IoBound:
          co_return co_await folly::coro::co_viaIfAsync(runtime->io_executor(),
                                                        std::move(task));
      }
      co_return co_await std::move(task);
    };
  }

  template <typename InputParams, typename OutputParams>
  void register_async_tool(
      const std::string& name, const std::string& description,
      const AsyncToolHandlerWithOutput<InputParams, OutputParams>& handler,
      ExecutionPolicy policy = ExecutionPolicy::CpuBound) {
    register_async_tool<InputParams>(
        name, description,
        [handler](const InputParams& params)
            -> folly::coro::Task<msg::types::CallToolResult> {
          const auto output = co_await handler(params);
          co_return utils::make_text_result(rfl::json::write(output));
        },
        policy);
  }

  msg::types::CallToolResult call_tool(const std::string& name,
                                       const rfl::Generic& params);
  folly::coro::Task<msg::types::CallToolResult> call_tool_async(
      const std::string& name, const rfl::Generic& params,
      folly::CancellationToken cancel_token = {});
  std::vector<msg::types::Tool> get_tool_list();

 private:
  std::map<std::string, AsyncToolHandlerInternal> tools_;
  std::map<std::string, msg::types::Tool> tool_descriptions_;
  std::shared_ptr<runtime::Runtime> runtime_;
};

}  // namespace phoenix_mcp::tool

#endif  // PHOENIX_MCP_TOOL_REGISTRY_TOOL_REGISTRY_H_
