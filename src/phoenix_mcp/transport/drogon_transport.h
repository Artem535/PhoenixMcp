#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "i_transport.h"
#include "../runtime/runtime.h"

namespace pxm::server {

class DrogonTransport final : public ITransport {
public:
  struct Config {
    std::string bind_address = "0.0.0.0";
    uint16_t port = 8080;
    std::string endpoint = "/mcp";
    std::string health_endpoint = "/health";
    int concurrency = 1;
  };

  DrogonTransport();
  explicit DrogonTransport(
      Config cfg,
      std::shared_ptr<runtime::Runtime> runtime =
          runtime::make_default_runtime());

  int run(Handler on_message) override;

private:
  Config cfg_;
  std::shared_ptr<runtime::Runtime> runtime_;
};

} // namespace pxm::server
