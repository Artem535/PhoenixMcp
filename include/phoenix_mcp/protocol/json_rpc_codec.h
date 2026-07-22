#ifndef PHOENIX_MCP_PROTOCOL_JSON_RPC_CODEC_H_
#define PHOENIX_MCP_PROTOCOL_JSON_RPC_CODEC_H_

#include <folly/Expected.h>

#include <string>
#include <variant>

#include "phoenix_mcp/core/error.h"
#include "phoenix_mcp/protocol/message_types.h"

namespace phoenix_mcp::protocol {

using JsonRpcMessage =
    std::variant<msg::types::Request, msg::types::Notification,
                 msg::types::Response, msg::types::Error>;

folly::Expected<msg::types::Request, core::McpError> decode_request(
    const std::string& json);

folly::Expected<msg::types::Notification, core::McpError>
decode_notification(const std::string& json);

folly::Expected<msg::types::Response, core::McpError> decode_response(
    const std::string& json);

folly::Expected<msg::types::Error, core::McpError> decode_error(
    const std::string& json);

folly::Expected<JsonRpcMessage, core::McpError> decode_message(
    const std::string& json);

folly::Expected<std::string, core::McpError> encode(
    const msg::types::Response& response);

folly::Expected<std::string, core::McpError> encode(
    const msg::types::Error& error);

folly::Expected<std::string, core::McpError> encode(
    const msg::types::Notification& notification);

}  // namespace phoenix_mcp::protocol

#endif  // PHOENIX_MCP_PROTOCOL_JSON_RPC_CODEC_H_