//
// Created by artem.d on 09.11.2025.
//

#include "stdio_transport.h"

#include <folly/coro/BlockingWait.h>

#include <iostream>
#include <spdlog/spdlog.h>


namespace pxm::server {
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

    const auto response = folly::coro::blockingWait(on_message(line));
    if (!response.has_value()) {
      continue;
    }

    if (*response == "null") {
      spdlog::error("StdioTransport| refusing to write 'null' to stdout");
      continue;
    }

    std::cout << *response << '\n';
    std::cout.flush();
  }
}
} // namespace pxm::server
