//
// Created by artem.d on 09.11.2025.
//

#include "server.h"

#include <iostream>

namespace pxm::server {

Server::Server(std::string name, std::string desc,
               std::unique_ptr<AbstractTransport>
               transport) : name_(std::move(name)), desc_(std::move(desc)),
                            transport_(std::move(transport)) {
}

int Server::start_server() {
  try {
    start_server_();
  } catch (std::exception& e) {
    std::cout << e.what() << std::endl;
    return cnt::exit::Error;
  }
  return cnt::exit::Success;
}

void Server::start_server_() {
  while (true) {
    std::string msg = transport_->read_msg();
    if (msg == "exit") {
      break;
    }
    transport_->write_msg(msg);
  }
}
}