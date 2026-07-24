//
// Created by artem.d on 09.11.2025.
//

#include "stdio_transport.h"

#include <folly/coro/BlockingWait.h>
#include <spdlog/spdlog.h>

#include <iostream>

namespace phoenix_mcp::server {
int StdioTransport::run(Handler on_message) {
  std::string line;
  while (true) {
    if (!std::getline(std::cin, line)) {
      spdlog::info("StdioTransport| stdin closed -> shutdown");
      return 0;
    }

    if (line.empty()) {
      spdlog::info("StdioTransport| empty line -> shutdown");
      return 0;
    }

    ITransport::RequestEnvelope request;
    request.body = std::move(line);
    const auto response =
        folly::coro::blockingWait(on_message(std::move(request)));
    if (!response.has_value()) {
      continue;
    }

    if (response->body == "null") {
      spdlog::error("StdioTransport| refusing to write 'null' to stdout");
      continue;
    }

    std::cout << response->body << '\n';
    std::cout.flush();
    line.clear();
  }
}
}  // namespace phoenix_mcp::server
