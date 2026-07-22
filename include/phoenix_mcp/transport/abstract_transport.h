#ifndef PHOENIX_MCP_TRANSPORT_ABSTRACT_TRANSPORT_H_
#define PHOENIX_MCP_TRANSPORT_ABSTRACT_TRANSPORT_H_

#include <string>

namespace phoenix_mcp::server {

class AbstractTransport {
 public:
  virtual ~AbstractTransport() = default;
  virtual std::string read_msg() = 0;
  virtual void write_msg(const std::string& msg) = 0;
};

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_TRANSPORT_ABSTRACT_TRANSPORT_H_
