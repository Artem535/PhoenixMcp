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
    // wanted (e.g. client disconnect). None of the current transports wire
    // this up yet, so it defaults to a token that never cancels; handlers
    // should merge it with any protocol-level cancellation source rather
    // than assume it's the only way a request gets cancelled.
    folly::CancellationToken cancel_token;
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
