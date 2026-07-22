#ifndef PHOENIX_MCP_CORE_ERROR_H_
#define PHOENIX_MCP_CORE_ERROR_H_

#include <rfl/Generic.hpp>

#include <memory>
#include <string>
#include <system_error>

namespace phoenix_mcp::core {

enum class ErrorCategory {
  Transport,
  Protocol,
  Serialization,
  Validation,
  Timeout,
  Cancellation,
  Lifecycle,
  Tool,
  Internal,
};

const std::error_category& mcp_error_category() noexcept;

enum class ErrorCode {
  Ok = 0,

  // Transport
  TransportClosed = 100,
  TransportReadFailed = 101,
  TransportWriteFailed = 102,

  // Protocol
  InvalidJson = 200,
  InvalidRequest = 201,
  MethodNotFound = 202,
  InvalidParams = 203,

  // Serialization
  SerializationFailed = 300,
  DeserializationFailed = 301,

  // Validation
  ValidationFailed = 400,

  // Timeout
  TimeoutExpired = 500,
  InitTimeoutExpired = 501,

  // Cancellation
  Cancelled = 600,

  // Lifecycle
  InvalidState = 700,
  AlreadyInitialized = 701,
  NotInitialized = 702,
  ShuttingDown = 703,

  // Tool
  ToolNotFound = 800,
  ToolExecutionFailed = 801,

  // Internal
  InternalError = 900,
};

std::error_code make_error_code(ErrorCode code) noexcept;

class McpError {
 public:
  McpError() = default;
  McpError(ErrorCode code, std::string message);
  McpError(ErrorCode code, std::string message, rfl::Generic data);
  McpError(ErrorCode code, std::string message, std::shared_ptr<McpError> cause);

  ErrorCode code() const noexcept;
  ErrorCategory category() const noexcept;
  const std::string& message() const noexcept;
  const rfl::Generic& data() const noexcept;
  const McpError* cause() const noexcept;

  std::error_code error_code() const noexcept;
  operator bool() const noexcept;

  std::string to_string() const;

 private:
  ErrorCode code_ = ErrorCode::Ok;
  std::string message_;
  rfl::Generic data_;
  std::shared_ptr<McpError> cause_;
};

}  // namespace phoenix_mcp::core

#endif  // PHOENIX_MCP_CORE_ERROR_H_