#ifndef PHOENIX_MCP_SERVER_SERVER_MESSAGE_SINK_H_
#define PHOENIX_MCP_SERVER_SERVER_MESSAGE_SINK_H_

#include <string>

#include <folly/coro/Task.h>

namespace phoenix_mcp::server {

/// @brief Delivers a server-initiated JSON-RPC message to one logical session.
///
/// Concrete transports decide how to deliver the serialized message. The
/// interface intentionally contains no HTTP, SSE, or framework-specific type.
class ServerMessageSink {
 public:
  virtual ~ServerMessageSink() = default;

  virtual folly::coro::Task<bool> publish(
      std::string session_key, std::string json_rpc_message) = 0;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_SERVER_SERVER_MESSAGE_SINK_H_
