#include "phoenix_mcp/protocol/json_rpc_codec.h"

#include <rfl/Generic.hpp>
#include <rfl/json.hpp>

namespace phoenix_mcp::protocol {

namespace {

using core::ErrorCode;
using core::McpError;

template <typename T>
folly::Expected<T, McpError> decode_json(const std::string& json) {
  auto result = rfl::json::read<T>(json);
  if (!result) {
    return folly::makeUnexpected(
        McpError(ErrorCode::DeserializationFailed,
                 "failed to parse JSON-RPC message"));
  }
  return std::move(result.value());
}

}  // namespace

folly::Expected<msg::types::Request, McpError> decode_request(
    const std::string& json) {
  return decode_json<msg::types::Request>(json);
}

folly::Expected<msg::types::Notification, McpError> decode_notification(
    const std::string& json) {
  return decode_json<msg::types::Notification>(json);
}

folly::Expected<msg::types::Response, McpError> decode_response(
    const std::string& json) {
  return decode_json<msg::types::Response>(json);
}

folly::Expected<msg::types::Error, McpError> decode_error(
    const std::string& json) {
  return decode_json<msg::types::Error>(json);
}

folly::Expected<JsonRpcMessage, McpError> decode_message(
    const std::string& json) {
  auto generic = rfl::json::read<rfl::Generic>(json);
  if (!generic) {
    return folly::makeUnexpected(
        McpError(ErrorCode::InvalidJson, "invalid JSON"));
  }

  auto obj_result = generic->to_object();
  if (!obj_result) {
    return folly::makeUnexpected(
        McpError(ErrorCode::InvalidJson, "JSON is not an object"));
  }
  const auto& obj = *obj_result;

  const auto has_id = obj.count("id") > 0;
  const auto has_method = obj.count("method") > 0;
  const auto has_result = obj.count("result") > 0;
  const auto has_error = obj.count("error") > 0;

  if (has_method && has_id && !has_result && !has_error) {
    auto req = decode_json<msg::types::Request>(json);
    if (!req) return folly::makeUnexpected(std::move(req.error()));
    return std::move(req.value());
  }

  if (has_method && !has_id && !has_result && !has_error) {
    auto notif = decode_json<msg::types::Notification>(json);
    if (!notif) return folly::makeUnexpected(std::move(notif.error()));
    return std::move(notif.value());
  }

  if (has_result && has_id && !has_method && !has_error) {
    auto resp = decode_json<msg::types::Response>(json);
    if (!resp) return folly::makeUnexpected(std::move(resp.error()));
    return std::move(resp.value());
  }

  if (has_error && has_id && !has_method && !has_result) {
    auto err = decode_json<msg::types::Error>(json);
    if (!err) return folly::makeUnexpected(std::move(err.error()));
    return std::move(err.value());
  }

  return folly::makeUnexpected(
      McpError(ErrorCode::InvalidRequest,
               "cannot determine JSON-RPC message type"));
}

folly::Expected<std::string, McpError> encode(
    const msg::types::Request& request) {
  try {
    return rfl::json::write(request);
  } catch (const std::exception& e) {
    return folly::makeUnexpected(
        McpError(ErrorCode::SerializationFailed, e.what()));
  }
}

folly::Expected<std::string, McpError> encode(
    const msg::types::Response& response) {
  try {
    return rfl::json::write(response);
  } catch (const std::exception& e) {
    return folly::makeUnexpected(
        McpError(ErrorCode::SerializationFailed, e.what()));
  }
}

folly::Expected<std::string, McpError> encode(
    const msg::types::Error& error) {
  try {
    return rfl::json::write(error);
  } catch (const std::exception& e) {
    return folly::makeUnexpected(
        McpError(ErrorCode::SerializationFailed, e.what()));
  }
}

folly::Expected<std::string, McpError> encode(
    const msg::types::Notification& notification) {
  try {
    return rfl::json::write(notification);
  } catch (const std::exception& e) {
    return folly::makeUnexpected(
        McpError(ErrorCode::SerializationFailed, e.what()));
  }
}

}  // namespace phoenix_mcp::protocol