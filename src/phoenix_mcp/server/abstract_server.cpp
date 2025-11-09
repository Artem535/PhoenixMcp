//
// Created by artem.d on 09.11.2025.
//

#include "abstract_server.h"

#include <iostream>

namespace pxm::server {
AbstractServer::AbstractServer(std::string name, std::string desc,
                               const cnt_t::TransportType& transport)
  : name_(std::move(name)), desc_(std::move(desc)), transport_(transport) {
}

int AbstractServer::start_server() {
  try {
    start_server_();
  } catch (std::exception& e) {
    std::cout << e.what() << std::endl;
    return cnt::exit::Error;
  }
  return cnt::exit::Success;
}

void AbstractServer::start_server_() {
  // Base implementation (can be empty or throw)
}
}