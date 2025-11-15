//
// Created by artem.d on 07.11.2025.
//

#pragma once
#include <array>
#include <string_view>

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

namespace pxm::msg::types::constants {

constexpr std::string_view initialize_request = "initialize";
constexpr std::string_view ping_request = "ping";
constexpr std::string_view list_tools_request = "tools/list";
constexpr std::string_view call_tool_request = "tools/call";
constexpr std::string_view cancel_notification = "notifications/cancelled";
constexpr std::string_view initialize_notification =
    "notifications/initialized";
constexpr std::string_view tool_list_changed_notification =
    "notification/tools/listChanged";
}