#ifndef PHOENIX_MCP_TRANSPORT_I_TRANSPORT_H_
#define PHOENIX_MCP_TRANSPORT_I_TRANSPORT_H_

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include <folly/CancellationToken.h>
#include <folly/coro/Task.h>

namespace phoenix_mcp::server {

class ITransport {
 public:
  struct RequestEnvelope {
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    // Cancelled if the transport itself detects the request is no longer
    // wanted (e.g. client disconnect). Transports that cannot detect a
    // disconnect leave it as a token that never cancels; handlers should merge
    // it with any protocol-level cancellation source rather than assume it is
    // the only way a request gets cancelled.
    folly::CancellationToken cancel_token;
    // Stable identity of the underlying connection this request arrived on,
    // so a session-aware handler (SessionManager) can route repeated
    // requests from the same connection to the same session. Empty for
    // transports where every request implicitly belongs to the same single
    // connection (e.g. stdio, which only ever serves one connection per
    // process); a session-aware handler treats an empty id as one shared
    // default connection rather than "no session."
    std::string connection_id;
    // True only when a stateful transport is attempting to bootstrap a new
    // logical session. The handler retains that session only if the request
    // successfully completes the MCP initialization handshake.
    bool session_bootstrap = false;
  };

  struct ResponseEnvelope {
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    int status_code = 0;  // 0 = success; non-zero is transport-interpreted.

    // Resolves the "0 means unset" convention above to a concrete status
    // code, so transports don't each repeat the same ternary.
    int status_or(int default_code) const {
      return status_code == 0 ? default_code : status_code;
    }
  };

  using Handler = std::function<folly::coro::Task<std::optional<ResponseEnvelope>>(
      RequestEnvelope)>;

  virtual ~ITransport() = default;
  virtual int run(Handler on_message) = 0;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_TRANSPORT_I_TRANSPORT_H_
