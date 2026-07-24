#ifndef PHOENIX_MCP_PROTOCOL_MESSAGE_TYPES_H_
#define PHOENIX_MCP_PROTOCOL_MESSAGE_TYPES_H_

#include <map>
#include <optional>
#include <rfl/Flatten.hpp>
#include <rfl/Generic.hpp>
#include <rfl/Rename.hpp>
#include <string>
#include <variant>
#include <vector>

namespace phoenix_mcp::msg::types {

using RequestId = std::variant<int, std::string>;
using OptionalParams = std::optional<rfl::Generic>;
using Result = rfl::Generic;
using EmptyResult = std::map<std::string, rfl::Generic>;
using ProgressToken = std::variant<int, std::string>;
using Cursor = std::string;
using LoggingLevel = std::string;
using Role = std::string;

struct Request {
  std::string jsonrpc = "2.0";
  std::string method;
  RequestId id;
  OptionalParams params;
};
struct MinimalRequest {
  std::string jsonrpc = "2.0";
  std::string method;
  RequestId id;
};
struct Response {
  std::string jsonrpc = "2.0";
  Result result;
  RequestId id;
};
struct Notification {
  std::string jsonrpc = "2.0";
  std::string method;
  OptionalParams params;
};
struct MinimalNotification {
  std::string jsonrpc = "2.0";
  std::string method;
};
struct ErrorData {
  int code;
  std::string message;
  rfl::Generic data;
};
struct Error {
  std::string jsonrpc = "2.0";
  RequestId id;
  ErrorData error;
};
struct PaginatedRequestParams {
  std::optional<Cursor> cursor;
};
struct PaginatedRequest {
  std::optional<PaginatedRequestParams> params;
};
struct InputSchema {
  std::string type = "object";
  OptionalParams properties;
  std::optional<std::vector<std::string>> required;
};
struct Tool {
  std::string name;
  std::optional<std::string> description;
  rfl::Rename<"inputSchema", InputSchema> input_schema;
};
struct ListToolsResult {
  std::vector<Tool> tools;
};
struct TextContent {
  std::string type = "text";
  std::string text;
};
struct ImageContent {
  std::string type = "image";
  std::string data;
  rfl::Rename<"mimeType", std::string> mime_type;
};
struct ResourceContent {
  std::string uri;
  rfl::Rename<"mimeType", std::optional<std::string>> mime_type;
};
struct TextResourceContent {
  rfl::Flatten<ResourceContent> flatten;
  std::string text;
};
struct BlobResourceContent {
  rfl::Flatten<ResourceContent> flatten;
  std::string blob;
};
struct EmbeddedResource {
  std::string type = "resource";
  std::variant<TextResourceContent, BlobResourceContent> resource;
};
using VariantContent =
    std::variant<TextContent, ImageContent, EmbeddedResource>;
struct CallToolResult {
  std::vector<VariantContent> content;
  rfl::Rename<"isError", std::optional<bool>> is_error;
};
struct CallToolParams {
  std::string name;
  OptionalParams arguments;
};
struct PropertySchema {
  std::string type;
};
struct ToolInputSchema {
  rfl::Rename<"$schema", std::string> schema;
  rfl::Rename<"$ref", std::string> ref;
  rfl::Rename<"$defs", std::map<std::string, InputSchema>> defs;
};
struct InitializeRequest {
  rfl::Flatten<Request> flatten;
};
struct PingRequest {
  rfl::Flatten<Request> flatten;
};
struct ListToolsRequest {
  rfl::Flatten<Request> flatten;
};
struct CallToolRequest {
  rfl::Flatten<MinimalRequest> flatten;
  std::optional<CallToolParams> params;
};
struct CancelNotificationParams {
  rfl::Rename<"requestId", RequestId> request_id;
  std::optional<std::string> reason;
};
struct CancelNotification {
  rfl::Flatten<MinimalNotification> flatten;
  std::optional<CancelNotificationParams> params;
};
struct InitializeNotification {
  rfl::Flatten<Notification> flatten;
};
struct ToolListChangedNotification {
  rfl::Flatten<Notification> flatten;
};
struct RootsParams {
  rfl::Rename<"listChanged", std::optional<bool>> list_changed;
};
struct ClientCapabilities {
  OptionalParams experimental;
  std::optional<RootsParams> roots;
  std::optional<rfl::Generic> sampling;
};
struct Implementation {
  std::string name;
  std::string version;
};
struct InitializeParams {
  rfl::Rename<"protocolVersion", std::string> protocol_version;
  ClientCapabilities capabilities;
  rfl::Rename<"clientInfo", Implementation> client_info;
};
struct PromptsCapabilities {
  rfl::Rename<"listChanged", std::optional<bool>> list_changed;
};
struct ResourcesCapabilities {
  rfl::Rename<"subscribe", std::optional<bool>> subscribe;
  rfl::Rename<"listChanged", std::optional<bool>> list_changed;
};
struct ToolsCapabilities {
  rfl::Rename<"listChanged", std::optional<bool>> list_changed;
};
struct ServerCapabilities {
  OptionalParams experimental;
  std::optional<rfl::Generic> logging;
  std::optional<PromptsCapabilities> prompts;
  std::optional<ResourcesCapabilities> resources;
  std::optional<ToolsCapabilities> tools;
};
struct InitializeResult {
  rfl::Rename<"protocolVersion", std::string> protocol_version;
  ServerCapabilities capabilities;
  rfl::Rename<"serverInfo", Implementation> server_info;
  std::optional<std::string> instruction;
};
struct InitializeResultRPC {
  std::string jsonrpc = "2.0";
  RequestId id;
  InitializeResult result;
};

}  // namespace phoenix_mcp::msg::types

#endif  // PHOENIX_MCP_PROTOCOL_MESSAGE_TYPES_H_
