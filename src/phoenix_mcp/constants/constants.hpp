//
// Created by artem.d on 07.11.2025.
//

#pragma once
#include <array>

namespace pxm::constants::transport {
enum class TransportType {
  Stdio,
  Http,
  Sse
};
}


namespace pxm::constants {
constexpr auto kMcpVersion = "2025-06-18";
constexpr std::array<transport::TransportType, 1> kSupportedTransport = {
    transport::TransportType::Stdio
};

}

namespace pxm::constants::exit {
enum Code {
  Success = 0,
  Error = 1
};

}

namespace pxm::constants::msg_error {
enum Code {
  Parse_error = -32700,
  Invalid_request = -32600,
  Method_not_found = -32601,
  Invalid_params = -32602,
  Internal_error = -32603,
};
}