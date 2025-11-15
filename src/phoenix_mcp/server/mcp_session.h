//
// Created by artem.d on 10.11.2025.
//

#pragma once

#include <chrono>

#include <rfl/Generic.hpp>
#include <rfl/json.hpp>
#include <spdlog/spdlog.h>

#include "../types/msg_types.hpp"
#include "../constants/constants.hpp"
#include "../tool_registry/tool_registry.h"


namespace pxm::server {

namespace ch = std::chrono;
namespace cnt_error = constants::msg_error;
namespace msg_t = msg::types;
using optional_request = std::optional<msg::types::Request>;
using optional_notification = std::optional<msg::types::Notification>;

/// @brief Class for managing MCP server session
/// Handles requests and notifications, manages server state
/// and coordinates tool registry operations
class McpSession {
public:
  /// @brief Constructor for server session
  /// @param server_capabilities Server capabilities (tools, resources, etc.)
  /// @param server_info Implementation info (name, version)
  /// @param instruction Server instruction
  /// @param tool_registry Unique pointer to tool registry
  McpSession(msg::types::ServerCapabilities server_capabilities,
             msg::types::Implementation server_info,
             std::string instruction,
             std::unique_ptr<tool::ToolRegistry> tool_registry);

  /// @brief Handle JSON request as string
  /// @param request JSON string containing the request
  /// @return Response in rfl::Generic format
  std::optional<rfl::Generic> handle_input(const std::string& request);

  /// @brief Handle structured request
  /// @param request Structured request object
  /// @return Response in rfl::Generic format
  rfl::Generic handle_request(const msg::types::Request& request);

private:
  /// @brief Server lifecycle stages
  enum class Stage {
    Uninitialized, ///< Server not initialized yet
    Initialized, ///< Server successfully initialized
    Operation, ///< Server in normal operation mode
    Shutdown ///< Server shutting down
  };

  ///< Initialization timeout (5 seconds)
  ch::time_point<ch::steady_clock> init_timeout_ =
      ch::steady_clock::now() + ch::seconds(5);
  ///< Current server stage
  Stage stage_ = Stage::Uninitialized;

  ///< Tool registry for managing available tools
  std::unique_ptr<tool::ToolRegistry> tool_registry_;
  ///< Server capabilities configuration
  msg::types::ServerCapabilities server_capabilities_;
  ///< Server implementation details
  msg::types::Implementation server_info_;
  ///< Server instruction text
  std::string instruction_;

  // ------ Functions ------
  /// @brief Check if initialization timeout has expired
  /// @return True if timeout has passed
  bool has_init_timeout() const;

  /// @brief Attempt to deserialize JSON string to Request object
  /// @param request JSON string to deserialize
  /// @return Optional request object, empty if deserialization failed
  static optional_request try_serialize_request(const std::string& request);

  /// @brief Handle initialization request
  /// @param request Initialization request object
  /// @return Response with initialization result
  rfl::Generic try_initialize(const msg::types::Request& request);

  template <class T>
  rfl::Generic make_response(const T& result,
                            const msg::types::RequestId& id) const;

  /// @brief Handle operational requests (tools, resources, etc.)
  /// @param request Request to handle
  /// @return Response with operation result
  rfl::Generic handle_operation(const msg::types::Request& request);

  /// @brief Create standardized error response
  /// @param msg Error message
  /// @param id Request ID for error correlation
  /// @param code Error code (default: invalid parameters)
  /// @return Error response in rfl::Generic format
  static rfl::Generic create_error(const std::string& msg,
                                   const msg::types::RequestId& id,
                                   int code = cnt_error::Code::Invalid_params);

  /// @brief Attempt to deserialize JSON string to Notification object
  /// @param json JSON string to deserialize
  /// @return Optional notification object, empty if deserialization failed
  static optional_notification try_serialize_notification(
      const std::string& json);

  /// @brief Handle incoming notification
  /// @param notif Notification object to process
  /// @return Response or empty if no response needed
  std::optional<rfl::Generic> handle_notification(
      const msg::types::Notification& notif);


  rfl::Generic call_tool(const msg::types::Request& request) const;
};
}