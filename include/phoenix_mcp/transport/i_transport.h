#ifndef PHOENIX_MCP_TRANSPORT_I_TRANSPORT_H_
#define PHOENIX_MCP_TRANSPORT_I_TRANSPORT_H_

#include <folly/coro/Task.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace phoenix_mcp::server {

class ITransport {
 public:
  struct RequestEnvelope {
    std::string body;
    std::unordered_map<std::string, std::string> headers;
  };

  using Handler = std::function<folly::coro::Task<std::optional<std::string>>(
      RequestEnvelope)>;

  virtual ~ITransport() = default;
  virtual int run(Handler on_message) = 0;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_TRANSPORT_I_TRANSPORT_H_
