#include "phoenix_mcp/server/stdio_server.h"


int main(int argc, char** argv) {
  pxm::server::StdioServer server{"", ""};
  return server.start_server();
}