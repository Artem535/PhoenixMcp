#include "phoenix_mcp/protocol/dialect.h"

#include <rfl/json.hpp>

namespace phoenix_mcp::protocol {

namespace {

using core::ErrorCode;
using core::McpError;

int to_jsonrpc_code(ErrorCode code) {
  switch (code) {
    case ErrorCode::InvalidJson:
      return -32700;
    case ErrorCode::InvalidRequest:
      return -32600;
    case ErrorCode::MethodNotFound:
      return -32601;
    case ErrorCode::InvalidParams:
      return -32602;
    case ErrorCode::InternalError:
      return -32603;
    default:
      return -32603;
  }
}

class Dialect2025_06_18 : public ProtocolDialect {
 public:
  ProtocolVersion version() const override { return "2025-06-18"; }

  folly::Expected<McpMessage, McpError> decode(
      const JsonRpcMessage& message) const override {
    if (auto* req = std::get_if<msg::types::Request>(&message)) {
      return decode_request(*req);
    }
    if (auto* notif = std::get_if<msg::types::Notification>(&message)) {
      return decode_notification(*notif);
    }
    return folly::makeUnexpected(
        McpError(ErrorCode::InvalidRequest,
                 "expected request or notification"));
  }

  folly::Expected<msg::types::Response, McpError> encode_result(
      const McpRequest& request,
      const rfl::Generic& result) const override {
    msg::types::RequestId id;
    if (auto* init = std::get_if<InitializeCall>(&request)) {
      id = init->request.flatten.get().id;
    } else if (std::get_if<PingCall>(&request)) {
      id = 0;
    } else if (auto* list = std::get_if<ListToolsCall>(&request)) {
      id = list->request.flatten.get().id;
    } else if (auto* call = std::get_if<CallToolCall>(&request)) {
      id = call->request.flatten.get().id;
    }
    return msg::types::Response{.jsonrpc = "2.0",
                                .result = result,
                                .id = id};
  }

  folly::Expected<msg::types::Error, McpError> encode_error(
      const msg::types::RequestId& id,
      const McpError& error) const override {
    return msg::types::Error{
        .jsonrpc = "2.0",
        .id = id,
        .error = msg::types::ErrorData{
            .code = to_jsonrpc_code(error.code()),
            .message = error.message()}};
  }

  CapabilityRules capability_rules() const override {
    return {.supports_tools = true,
            .supports_resources = false,
            .supports_prompts = false,
            .supports_logging = false};
  }

 private:
  folly::Expected<McpMessage, McpError> decode_request(
      const msg::types::Request& req) const {
    if (req.method == "initialize") {
      auto init =
          rfl::json::read<msg::types::InitializeRequest>(
              rfl::json::write(req));
      if (!init) {
        return folly::makeUnexpected(McpError(
            ErrorCode::DeserializationFailed,
            "failed to decode initialize request"));
      }
      return McpRequest{InitializeCall{init.value()}};
    }

    if (req.method == "ping") {
      return McpRequest{PingCall{}};
    }

    if (req.method == "tools/list") {
      auto list =
          rfl::json::read<msg::types::ListToolsRequest>(
              rfl::json::write(req));
      if (!list) {
        return folly::makeUnexpected(McpError(
            ErrorCode::DeserializationFailed,
            "failed to decode tools/list request"));
      }
      return McpRequest{ListToolsCall{list.value()}};
    }

    if (req.method == "tools/call") {
      McpRequest result_req;
      auto call =
          rfl::json::read<msg::types::CallToolRequest>(
              rfl::json::write(req));
      if (call) {
        result_req = McpRequest{CallToolCall{call.value()}};
      } else {
        return folly::makeUnexpected(McpError(
            ErrorCode::DeserializationFailed,
            "failed to decode tools/call request"));
      }
      return result_req;
    }

    return folly::makeUnexpected(McpError(
        ErrorCode::MethodNotFound,
        "unknown method: " + req.method));
  }

  folly::Expected<McpMessage, McpError> decode_notification(
      const msg::types::Notification& notif) const {
    if (notif.method == "notifications/cancelled") {
      auto cancel =
          rfl::json::read<msg::types::CancelNotification>(
              rfl::json::write(notif));
      if (!cancel) {
        return folly::makeUnexpected(McpError(
            ErrorCode::DeserializationFailed,
            "failed to decode cancel notification"));
      }
      return McpNotification{CancelNotificationCall{cancel.value()}};
    }

    if (notif.method == "notifications/initialized") {
      auto init =
          rfl::json::read<msg::types::InitializeNotification>(
              rfl::json::write(notif));
      if (!init) {
        return folly::makeUnexpected(McpError(
            ErrorCode::DeserializationFailed,
            "failed to decode initialized notification"));
      }
      return McpNotification{
          InitializeNotificationCall{init.value()}};
    }

    if (notif.method == "notification/tools/listChanged") {
      auto changed =
          rfl::json::read<msg::types::ToolListChangedNotification>(
              rfl::json::write(notif));
      if (!changed) {
        return folly::makeUnexpected(McpError(
            ErrorCode::DeserializationFailed,
            "failed to decode tools list changed notification"));
      }
      return McpNotification{
          ToolListChangedNotificationCall{changed.value()}};
    }

    return folly::makeUnexpected(McpError(
        ErrorCode::MethodNotFound,
        "unknown notification: " + notif.method));
  }
};

}  // namespace

std::unique_ptr<ProtocolDialect> make_dialect_2025_06_18() {
  return std::make_unique<Dialect2025_06_18>();
}

std::unique_ptr<ProtocolDialect> make_dialect(
    const ProtocolVersion& version) {
  if (version == "2025-06-18") {
    return make_dialect_2025_06_18();
  }
  return nullptr;
}

}  // namespace phoenix_mcp::protocol