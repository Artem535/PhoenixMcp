#pragma once

#include <folly/coro/Task.h>

#include <functional>
#include <unordered_map>
#include <optional>
#include <string>

namespace pxm::server {

class ITransport {
public:
  struct RequestEnvelope {
    std::string body;
    std::unordered_map<std::string, std::string> headers;
  };

  using Handler = std::function<
      folly::coro::Task<std::optional<std::string>>(RequestEnvelope)>;

  virtual ~ITransport() = default;
  virtual int run(Handler on_message) = 0;
};

} // namespace pxm::server
