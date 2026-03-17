#pragma once

#include <memory>
#include <utility>

#include "../transport/i_transport.h"
#include "mcp_request_handler.h"

namespace pxm::server {

class McpServer {
public:
  McpServer(std::unique_ptr<ITransport> transport,
            std::unique_ptr<McpRequestHandler> handler)
      : transport_(std::move(transport)), handler_(std::move(handler)) {
  }

  int run() {
    return transport_->run([this](ITransport::RequestEnvelope request) {
      return handler_->handle_json_async(std::move(request));
    });
  }

private:
  std::unique_ptr<ITransport> transport_;
  std::unique_ptr<McpRequestHandler> handler_;
};

} // namespace pxm::server
