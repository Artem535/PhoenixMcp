//
// Created by artem.d on 10.11.2025.
//

#include "mcp_session.h"

#include <utility>

namespace pxm::server {

McpSession::McpSession(
    msg::types::ServerCapabilities server_capabilities,
    msg::types::Implementation server_info,
    std::string instruction,
    std::unique_ptr<tool::ToolRegistry> tool_registry) : tool_registry_(
    std::move(tool_registry)), server_capabilities_(std::move(
    server_capabilities)), server_info_(std::move(server_info)), instruction_(
    std::move(
        instruction)) {
}

std::optional<rfl::Generic>
McpSession::handle_input(const std::string& request) {
  if (const auto req = try_serialize_request(request); req.has_value())
    return handle_request(*req);

  if (const auto notif = try_serialize_notification(request); notif.has_value())
    return handle_notification(*notif);

  spdlog::error("handle_input| Parsing Error");
  return std::nullopt;
}

rfl::Generic McpSession::handle_request(const msg::types::Request& request) {
  if (has_init_timeout()) {
    return create_error("Initialization timeout", request.id.value());
  }

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

  spdlog::error("McpSession::handle_input| Invalid stage");
  return create_error("Something went wrong", request.id.value());
}

bool McpSession::has_init_timeout() const {
  const bool is_correct_stage = stage_ == Stage::Initialized;
  const bool is_timeout = std::chrono::steady_clock::now() > init_timeout_;
  return is_correct_stage && is_timeout;
}

std::optional<msg::types::Request> McpSession::try_serialize_request(
    const std::string& request) {

  std::optional<msg::types::Request> request_ = std::nullopt;
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
  spdlog::info("McpSession| Switch to initialized stage.");

  const msg::types::InitializeResult result{
      .protocol_version = constants::kMcpVersion,
      .capabilities = server_capabilities_,
      .server_info = server_info_,
      .instruction = instruction_
  };

  const msg::types::InitializeResultRPC resp{
      .id = request.id.value(),
      .result = result
  };

  return rfl::to_generic(resp);
}

template <typename T>
rfl::Generic McpSession::make_response(const T& result,
                                       const msg::types::RequestId& id) const {

  const msg::types::Response resp{
      .jsonrpc = "2.0",
      .result = rfl::to_generic(result),
      .id = id
  };

  return rfl::to_generic(resp);
}

rfl::Generic McpSession::handle_operation(const msg::types::Request& request) {
  if (request.method.value() == msg_t::ListToolsRequest::method) {
    const auto tool_list = tool_registry_->get_tool_list();
    const auto tool_list_res = msg_t::ListToolsResult{.tools = tool_list};
    return make_response(tool_list_res, request.id.value());
  }

  // TODO: Refactor this
  if (request.method.value() == msg_t::CallToolRequest::method) {
    try {
      spdlog::debug("McpSession::handle_operation| Call tool {}",
                    rfl::json::write(request));

      if (const auto& optional_generic_params = request.params.value();
        optional_generic_params.has_value()) {
        auto generic_params = optional_generic_params.value().to_object().
            value().get("params").value();

        const auto [name, arguments] = rfl::from_generic<msg_t::CallToolParams>(
            generic_params).value();
        const auto generic_args = rfl::to_generic(arguments.value());

        spdlog::debug("McpSession::handle_operation| Call tool, name: {}",
                      name.value());
        spdlog::debug("McpSession::handle_operation| Call tool, args: {}",
                      rfl::json::write(generic_args));
        const auto result = tool_registry_->
            call_tool(name.value(), generic_args);

        return make_response(result, request.id.value());
      }

    } catch (const std::exception& e) {
      return create_error(e.what(), request.id.value(), -32602);
    }
  }

  return create_error("Method not found", request.id.value(), -32601);
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

std::optional<msg::types::Notification>
McpSession::try_serialize_notification(const std::string& json) {
  try {
    return rfl::json::read<msg::types::Notification>(json).value();
  } catch (...) {
    return std::nullopt;
  }
}


// TODO: You must be void?
std::optional<rfl::Generic> McpSession::handle_notification(
    const msg::types::Notification& notif) {

  const bool is_initialize = stage_ == Stage::Initialized;
  const bool is_correct_method = notif.method.value() ==
                                 msg::types::InitializeNotification::method;
  spdlog::debug("McpSession| is_initialize: {}, is_correct_method: {}",
                is_initialize, is_correct_method);

  if (is_initialize && is_correct_method) {
    stage_ = Stage::Operation;
    spdlog::info("McpSession| Switch to operation stage");
  }

  return std::nullopt;
}

}