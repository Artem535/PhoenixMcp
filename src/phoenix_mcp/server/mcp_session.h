//
// Created by artem.d on 10.11.2025.
//

#pragma once

#include <rfl/Generic.hpp>
#include <rfl/json.hpp>
#include <spdlog/spdlog.h>

#include "../types/msg_types.hpp"
#include "../constants/constants.hpp"
#include "../tool_registry/tool_registry.h"


namespace pxm::server {

namespace cnt_error = pxm::constants::msg_error;

class McpSession {
public:
  McpSession(const msg::types::ServerCapabilities& server_capabilities,
             const msg::types::Implementation& server_info,
             const std::string& instruction,
             std::unique_ptr<tool::ToolRegistry> tool_registry);

  rfl::Generic handle_request(const std::string& request);

  rfl::Generic handle_request(const msg::types::Request& request);

private:
  enum class Stage {
    Uninitialized,
    Initialized,
    Operation,
    Shutdown
  };

  Stage stage_ = Stage::Uninitialized; ///< Current server stage
  std::unique_ptr<tool::ToolRegistry> tool_registry_;
  msg::types::ServerCapabilities server_capabilities_;
  msg::types::Implementation server_info_;
  std::string instruction_;

  // ------ Functions ------
  static std::optional<msg::types::Request> try_serialize_request(
      const std::string& request);

  rfl::Generic try_initialize(const msg::types::Request& request);

  rfl::Generic handle_operation(const msg::types::Request& request);

  static rfl::Generic create_error(const std::string& msg,
                                   const msg::types::RequestId& id,
                                   int code = cnt_error::Code::Invalid_params);

  static
  std::optional<msg::types::Notification> try_serialize_notification(
      const std::string& json);

  rfl::Generic handle_notification(
      const msg::types::Notification &notif);

};
}