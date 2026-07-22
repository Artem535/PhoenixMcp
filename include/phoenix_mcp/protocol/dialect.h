#ifndef PHOENIX_MCP_PROTOCOL_DIALECT_H_
#define PHOENIX_MCP_PROTOCOL_DIALECT_H_

#include <memory>
#include <string>
#include <variant>

#include <folly/Expected.h>

#include "phoenix_mcp/core/error.h"
#include "phoenix_mcp/protocol/json_rpc_codec.h"
#include "phoenix_mcp/protocol/message_types.h"

namespace phoenix_mcp::protocol {

using ProtocolVersion = std::string;

struct InitializeCall {
  msg::types::InitializeRequest request;
};
struct PingCall {
  msg::types::RequestId id;
};
struct ListToolsCall {
  msg::types::ListToolsRequest request;
};
struct CallToolCall {
  msg::types::CallToolRequest request;
};
struct CancelNotificationCall {
  msg::types::CancelNotification notification;
};
struct InitializeNotificationCall {
  msg::types::InitializeNotification notification;
};
struct ToolListChangedNotificationCall {
  msg::types::ToolListChangedNotification notification;
};

using McpRequest =
    std::variant<InitializeCall, PingCall, ListToolsCall, CallToolCall>;
using McpNotification =
    std::variant<CancelNotificationCall, InitializeNotificationCall,
                 ToolListChangedNotificationCall>;

using McpMessage = std::variant<McpRequest, McpNotification>;

struct CapabilityRules {
  bool supports_tools = false;
  bool supports_resources = false;
  bool supports_prompts = false;
  bool supports_logging = false;
};

class ProtocolDialect {
 public:
  virtual ~ProtocolDialect() = default;

  virtual ProtocolVersion version() const = 0;

  virtual folly::Expected<McpMessage, core::McpError> decode(
      const JsonRpcMessage& message) const = 0;

  virtual folly::Expected<msg::types::Response, core::McpError>
  encode_result(const McpRequest& request,
                const rfl::Generic& result) const = 0;

  virtual folly::Expected<msg::types::Error, core::McpError>
  encode_error(const msg::types::RequestId& id,
               const core::McpError& error) const = 0;

  virtual CapabilityRules capability_rules() const = 0;
};

std::unique_ptr<ProtocolDialect> make_dialect_2025_06_18();

std::unique_ptr<ProtocolDialect> make_dialect(
    const ProtocolVersion& version);

}  // namespace phoenix_mcp::protocol

#endif  // PHOENIX_MCP_PROTOCOL_DIALECT_H_