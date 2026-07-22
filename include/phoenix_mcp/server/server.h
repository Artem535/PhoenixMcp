#ifndef PHOENIX_MCP_SERVER_SERVER_H_
#define PHOENIX_MCP_SERVER_SERVER_H_

#include <memory>
#include <string>

#include "phoenix_mcp/protocol/message_types.h"
#include "phoenix_mcp/server/mcp_request_handler.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "phoenix_mcp/transport/abstract_transport.h"

namespace phoenix_mcp::server {

class Server {
 public:
  Server(std::string name, std::string version,
         std::unique_ptr<AbstractTransport> transport,
         std::unique_ptr<tool::ToolRegistry> tool_registry,
         std::string instruction);

  int start_server();
  void change_tool_registry(std::unique_ptr<tool::ToolRegistry> tool_registry);

 private:
  std::string name_;
  std::string desc_;
  msg::types::Implementation server_info_;
  std::string instruction_;
  msg::types::ServerCapabilities server_capabilities_;
  std::unique_ptr<McpRequestHandler> request_handler_;
  std::unique_ptr<AbstractTransport> transport_;

  void start_server_();
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_SERVER_SERVER_H_
