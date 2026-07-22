//
// Created by artem.d on 18.02.2026.
//

#include "mcp_request_handler.h"

#include <folly/coro/BlockingWait.h>

#include "mcp_session.h"
#if PXM_WITH_OTEL
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/nostd/string_view.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#endif
#include <spdlog/spdlog.h>

#if PXM_WITH_OTEL
#include <array>
#include <cctype>
#endif
#include <utility>
#include <vector>

namespace phoenix_mcp::server {
namespace {

#if PXM_WITH_OTEL
class HeaderCarrier final
    : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  explicit HeaderCarrier(
      const std::unordered_map<std::string, std::string>& headers)
      : headers_(headers) {}

  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view key) const noexcept override {
    const std::string key_string =
        to_lower(std::string(key.data(), key.size()));
    const auto it = headers_.find(key_string);
    if (it == headers_.end()) {
      return {};
    }
    return it->second;
  }

  void Set(opentelemetry::nostd::string_view,
           opentelemetry::nostd::string_view) noexcept override {}

  bool Keys(opentelemetry::nostd::function_ref<
            bool(opentelemetry::nostd::string_view)>
                callback) const noexcept override {
    for (const auto& [key, _] : headers_) {
      if (!callback(key)) {
        return false;
      }
    }
    return true;
  }

 private:
  static std::string to_lower(std::string value) {
    for (char& ch : value) {
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
  }

  const std::unordered_map<std::string, std::string>& headers_;
};

opentelemetry::context::Context extract_context(
    const std::unordered_map<std::string, std::string>& headers) {
  if (headers.empty()) {
    return opentelemetry::context::RuntimeContext::GetCurrent();
  }

  static auto trace_context_propagator = opentelemetry::nostd::shared_ptr<
      opentelemetry::context::propagation::TextMapPropagator>(
      new opentelemetry::trace::propagation::HttpTraceContext());
  HeaderCarrier carrier(headers);
  auto current_context = opentelemetry::context::RuntimeContext::GetCurrent();
  return trace_context_propagator->Extract(carrier, current_context);
}

std::string trace_id_to_string(
    const opentelemetry::trace::SpanContext& span_context) {
  if (!span_context.trace_id().IsValid()) {
    return "invalid";
  }

  std::array<char, opentelemetry::trace::TraceId::kSize * 2> buffer{};
  span_context.trace_id().ToLowerBase16(buffer);
  return std::string(buffer.data(), buffer.size());
}
#endif

}  // namespace

McpRequestHandler::McpRequestHandler(
    msg::types::ServerCapabilities server_capabilities,
    msg::types::Implementation server_info, std::string instruction,
    std::unique_ptr<tool::ToolRegistry> tool_registry)
    : session_(std::make_unique<McpSession>(
          std::move(server_capabilities), std::move(server_info),
          std::move(instruction), std::move(tool_registry))) {}

McpRequestHandler::~McpRequestHandler() = default;

std::optional<std::string> McpRequestHandler::handle_json(
    const std::string& request_json) {
  return folly::coro::blockingWait(handle_json_async(request_json));
}

std::optional<std::string> McpRequestHandler::handle_json(
    const ITransport::RequestEnvelope& request) {
  return folly::coro::blockingWait(handle_json_async(request));
}

folly::coro::Task<std::optional<std::string>>
McpRequestHandler::handle_json_async(std::string request_json) {
  ITransport::RequestEnvelope request;
  request.body = std::move(request_json);
  co_return co_await handle_json_async(std::move(request));
}

folly::coro::Task<std::optional<std::string>>
McpRequestHandler::handle_json_async(ITransport::RequestEnvelope request) {
#if PXM_WITH_OTEL
  const auto traceparent_it = request.headers.find("traceparent");
  const auto tracestate_it = request.headers.find("tracestate");
  const auto extracted_context = extract_context(request.headers);
  const auto extracted_span =
      opentelemetry::trace::GetSpan(extracted_context)->GetContext();
  spdlog::info(
      "McpRequestHandler::handle_json_async| trace context extracted "
      "traceparent={} tracestate={} extracted_trace_id={}",
      traceparent_it != request.headers.end() ? traceparent_it->second : "",
      tracestate_it != request.headers.end() ? tracestate_it->second : "",
      trace_id_to_string(extracted_span));
  auto context_guard =
      opentelemetry::context::RuntimeContext::Attach(extracted_context);
#endif
  const auto result =
      co_await session_->handle_input_async(std::move(request.body));
  if (!result.has_value()) {
    co_return std::nullopt;
  }

  co_return rfl::json::write(result.value());
}
}  // namespace phoenix_mcp::server
