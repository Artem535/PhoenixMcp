#include "phoenix_mcp/core/error.h"

namespace phoenix_mcp::core {

namespace {

class McpErrorCategory : public std::error_category {
 public:
  const char* name() const noexcept override { return "phoenix_mcp"; }

  std::error_condition default_error_condition(
      int code) const noexcept override {
    return std::error_condition(code, *this);
  }

  bool equivalent(int code,
                  const std::error_condition& cond) const noexcept override {
    return default_error_condition(code) == cond;
  }

  bool equivalent(const std::error_code& ec,
                  int code) const noexcept override {
    return *this == ec.category() && ec.value() == code;
  }

  std::string message(int code) const noexcept override {
    switch (static_cast<ErrorCode>(code)) {
      case ErrorCode::Ok:
        return "no error";
      case ErrorCode::TransportClosed:
        return "transport closed";
      case ErrorCode::TransportReadFailed:
        return "transport read failed";
      case ErrorCode::TransportWriteFailed:
        return "transport write failed";
      case ErrorCode::InvalidJson:
        return "invalid JSON";
      case ErrorCode::InvalidRequest:
        return "invalid request";
      case ErrorCode::MethodNotFound:
        return "method not found";
      case ErrorCode::InvalidParams:
        return "invalid params";
      case ErrorCode::SerializationFailed:
        return "serialization failed";
      case ErrorCode::DeserializationFailed:
        return "deserialization failed";
      case ErrorCode::ValidationFailed:
        return "validation failed";
      case ErrorCode::TimeoutExpired:
        return "timeout expired";
      case ErrorCode::InitTimeoutExpired:
        return "init timeout expired";
      case ErrorCode::Cancelled:
        return "cancelled";
      case ErrorCode::InvalidState:
        return "invalid state";
      case ErrorCode::AlreadyInitialized:
        return "already initialized";
      case ErrorCode::NotInitialized:
        return "not initialized";
      case ErrorCode::ShuttingDown:
        return "shutting down";
      case ErrorCode::ToolNotFound:
        return "tool not found";
      case ErrorCode::ToolExecutionFailed:
        return "tool execution failed";
      case ErrorCode::InternalError:
        return "internal error";
      case ErrorCode::AuthenticationFailed:
        return "authentication failed";
      case ErrorCode::AuthorizationFailed:
        return "authorization failed";
      default:
        return "unknown error";
    }
  }
};

const McpErrorCategory& mcp_category() {
  static McpErrorCategory instance;
  return instance;
}

}  // namespace

const std::error_category& mcp_error_category() noexcept {
  return mcp_category();
}

std::error_code make_error_code(ErrorCode code) noexcept {
  return {static_cast<int>(code), mcp_error_category()};
}

McpError::McpError(ErrorCode code, std::string message)
    : code_(code), message_(std::move(message)) {}

McpError::McpError(ErrorCode code, std::string message, rfl::Generic data)
    : code_(code),
      message_(std::move(message)),
      data_(std::move(data)) {}

McpError::McpError(ErrorCode code, std::string message,
                   std::shared_ptr<McpError> cause)
    : code_(code),
      message_(std::move(message)),
      cause_(std::move(cause)) {}

McpError::McpError(ErrorCode code, std::string message, rfl::Generic data,
                   std::shared_ptr<McpError> cause)
    : code_(code),
      message_(std::move(message)),
      data_(std::move(data)),
      cause_(std::move(cause)) {}

ErrorCode McpError::code() const noexcept { return code_; }

ErrorCategory McpError::category() const noexcept {
  int c = static_cast<int>(code_);
  if (c >= 100 && c < 200) return ErrorCategory::Transport;
  if (c >= 200 && c < 300) return ErrorCategory::Protocol;
  if (c >= 300 && c < 400) return ErrorCategory::Serialization;
  if (c >= 400 && c < 500) return ErrorCategory::Validation;
  if (c >= 500 && c < 600) return ErrorCategory::Timeout;
  if (c >= 600 && c < 700) return ErrorCategory::Cancellation;
  if (c >= 700 && c < 800) return ErrorCategory::Lifecycle;
  if (c >= 800 && c < 900) return ErrorCategory::Tool;
  if (c >= 900 && c < 1000) return ErrorCategory::Internal;
  if (c >= 1000 && c < 1100) return ErrorCategory::Authentication;
  return ErrorCategory::Authorization;
}

const std::string& McpError::message() const noexcept { return message_; }

const rfl::Generic& McpError::data() const noexcept { return data_; }

const McpError* McpError::cause() const noexcept {
  return cause_ ? cause_.get() : nullptr;
}

std::error_code McpError::error_code() const noexcept {
  return make_error_code(code_);
}

McpError::operator bool() const noexcept {
  return code_ != ErrorCode::Ok;
}

std::string McpError::to_string() const {
  std::string result = error_code().message();
  if (!message_.empty()) {
    result += ": " + message_;
  }
  if (cause_) {
    result += "\n  caused by: " + cause_->to_string();
  }
  return result;
}

}  // namespace phoenix_mcp::core