#include "stdio_server.h"

#include <iostream>

namespace pxm::server {


StdioServer::StdioServer(const std::string& name,
                         const std::string& description)
  : AbstractServer(name, description) {
}

void StdioServer::start_server_() {
  // Stdio-specific implementation
}

} // namespace pxm::server