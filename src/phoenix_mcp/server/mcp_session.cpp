//
// Created by artem.d on 10.11.2025.
//

#include "mcp_session.h"

namespace pxm::server {

McpSession::McpSession(
    const msg::types::ServerCapabilities& server_capabilities,
    const msg::types::Implementation& server_info,
    const std::string& instruction,
    std::unique_ptr<tool::ToolRegistry> tool_registry) : tool_registry_(
      std::move(tool_registry)), server_capabilities_(
      server_capabilities), server_info_(server_info), instruction_(
      instruction) {
}

std::optional<msg::types::Notification>
McpSession::try_serialize_notification(const std::string& json) {
  try {
    return rfl::json::read<msg::types::Notification>(json).value();
  } catch (...) {
    return std::nullopt;
  }

}

rfl::Generic McpSession::handle_notification(
    const msg::types::Notification& notif) {

  const bool is_initialize = stage_ == Stage::Initialized;
  const bool is_correct_method = notif.method.value() ==
                                 msg::types::InitializeNotification::method;

  if (is_initialize && is_correct_method) {
    stage_ = Stage::Operation;
  }

  return {};
}

rfl::Generic McpSession::handle_request(const std::string& request) {
  if (const auto req = try_serialize_request(request); req) {
    return handle_request(*req);
  }

  if (const auto notif = try_serialize_notification(request); notif) {
    return handle_notification(*notif);
  }

  return rfl::Generic{};
}

rfl::Generic McpSession::handle_request(const msg::types::Request& request) {

  switch (stage_) {
    case Stage::Uninitialized:
      return try_initialize(request);
    case Stage::Operation:
      return handle_operation(request);
    case Stage::Initialized:
      return create_error("Waiting for 'notifications/initialized'",
                          request.id.value());
    case Stage::Shutdown:
      return create_error("Server is shutting down", request.id.value());
  }

  spdlog::error("McpSession::handle_request| Invalid stage");
  return create_error("Something went wrong", request.id.value());
}

std::optional<msg::types::Request> McpSession::try_serialize_request(
    const std::string& request) {

  std::optional<msg::types::Request> request_;
  try {
    request_ = rfl::json::read<msg::types::Request>(request).value();
  } catch (const std::exception& e) {
    spdlog::error("Failed to serialize request: {}", e.what());
  }

  return request_;
}

rfl::Generic McpSession::try_initialize(const msg::types::Request& request) {
  // Check correct msg init method.
  if (request.method.value() != msg::types::InitializeRequest::method) {
    return create_error("Invalid request method", request.id.value());
  }

  // Change current stage to initialized.
  stage_ = Stage::Initialized;

  const msg::types::InitializeResult result{
      .protocol_version = constants::kMcpVersion,
      .capabilities = server_capabilities_,
      .server_info = server_info_,
      .instruction = instruction_
  };

  return rfl::to_generic(result);
}

rfl::Generic McpSession::handle_operation(const msg::types::Request& request) {

}

rfl::Generic McpSession::create_error(const std::string& msg,
                                      const msg::types::RequestId& id,
                                      int code) {
  const msg::types::Error error{
      .id = id,
      .error = msg::types::ErrorData{
          .code = code,
          .message = msg
      }
  };

  return rfl::to_generic(error);
}

}