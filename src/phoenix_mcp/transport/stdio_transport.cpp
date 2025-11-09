//
// Created by artem.d on 09.11.2025.
//

#include "stdio_transport.h"
#include <iostream>


namespace pxm::server {
std::string StdioTransport::read_msg() {
  std::string msg;
  std::getline(std::cin, msg);
  return msg;
}

void StdioTransport::write_msg(const std::string& msg) {
  std::cout << msg << '\n';
}
}