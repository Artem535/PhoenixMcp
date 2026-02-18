#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "i_transport.h"

namespace pxm::server {

class CrowTransport final : public ITransport {
public:
  struct Config {
    std::string bind_address = "0.0.0.0";
    uint16_t port = 8080;
    std::string endpoint = "/mcp";
    std::string health_endpoint = "/health";
    int concurrency = 1;
  };

  CrowTransport();
  explicit CrowTransport(Config cfg);

  int run(Handler on_message) override;

private:
  Config cfg_;
};

} // namespace pxm::server
