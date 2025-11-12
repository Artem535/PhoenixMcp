//
// Created by artem.d on 07.11.2025.
//
#pragma once

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <rfl/Rename.hpp>
#include <rfl/Generic.hpp>
#include <rfl/Flatten.hpp>


namespace pxm::msg::types {

/// @brief Type alias for request identifiers, which can be either integer or string
using RequestId = std::variant<int, std::string>;

/// @brief Type alias for optional parameters map with generic values
using OptionalParams = std::optional<rfl::Generic>;

/// @brief Type alias for result map with generic values
using Result = rfl::Generic;

/// @brief Type alias for empty result (same structure as Result)
using EmptyResult = std::map<std::string, rfl::Generic>;

/// @brief Type alias for progress tokens, which can be either integer or string
using ProgressToken = std::variant<int, std::string>;

/// @brief Type alias for cursor strings used in pagination
using Cursor = std::string;

/// @brief Type alias for logging level strings
using LoggingLevel = std::string;

/// @brief Type alias for role strings
using Role = std::string;

/* --------------- JSON-RPC structures --------------- */

/// @brief Structure representing a JSON-RPC request
struct Request {
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  rfl::Rename<"method", std::string> method;
  rfl::Rename<"id", RequestId> id;
  rfl::Rename<"params", OptionalParams> params;
};

/// @brief Structure representing a JSON-RPC response
struct Response {
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  rfl::Rename<"result", Result> result;
  rfl::Rename<"id", RequestId> id;
};

/// @brief Structure representing a JSON-RPC notification
struct Notification {
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  rfl::Rename<"method", std::string> method;
  rfl::Rename<"params", OptionalParams> params;
};

/// @brief Structure containing error details
struct ErrorData {
  rfl::Rename<"code", int> code;
  rfl::Rename<"message", std::string> message;
  rfl::Rename<"data", rfl::Generic> data;
};

/// @brief Structure representing a JSON-RPC error response
struct Error {
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  rfl::Rename<"id", RequestId> id;
  rfl::Rename<"error", ErrorData> error;
};

/* --------------- requests --------------- */

/// @brief Structure representing an initialize request
struct InitializeRequest {
  rfl::Flatten<Request> flatten;
  static constexpr std::string_view method = "initialize";
};

/// @brief Structure representing a ping request
struct PingRequest {
  rfl::Flatten<Request> flatten;
  static constexpr std::string_view method = "ping";
};

/// @brief Structure for requesting a list of tools
struct ListToolsRequest {
  rfl::Flatten<Request> flatten;
  static constexpr std::string_view method = "tools/list";
};

/// @brief Structure representing a tool call request
struct CallToolRequest {
  rfl::Flatten<Request> flatten;
  static constexpr std::string_view method = "tools/call";
};

/* --------------- notifications--------------- */

/// @brief Structure representing a cancellation notification
struct CancelNotification {
  rfl::Flatten<Notification> flatten;
  static constexpr std::string_view method = "notifications/cancelled";
};

/// @brief Structure representing an initialization notification
struct InitializeNotification {
  rfl::Flatten<Notification> flatten;
  static constexpr std::string_view method = "notifications/initialized";
};

/// @brief Structure representing a notification when the tool list changes
struct ToolListChangedNotification {
  rfl::Flatten<Notification> flatten;
  static constexpr std::string_view method = "notification/tools/listChanged";
};

/* --------------- different structures --------------- */

/// @brief Structure for notification parameters containing request ID and optional reason
struct NotificationParams {
  rfl::Rename<"requestId", RequestId> request_id;
  rfl::Rename<"reason", std::optional<std::string> > reason;
};

/// @brief Structure containing roots capability parameters
struct RootsParams {
  rfl::Rename<"listChanged", std::optional<bool> > list_changed;
};

/// @brief Structure representing client capabilities
struct ClientCapabilities {
  rfl::Rename<"experimental", OptionalParams> experimental;
  rfl::Rename<"roots", std::optional<RootsParams> > roots;
  rfl::Rename<"sampling", std::optional<rfl::Generic> > sampling;
};

/// @brief Structure representing implementation details (name and version)
struct Implementation {
  rfl::Rename<"name", std::string> name;
  rfl::Rename<"version", std::string> version;
};

/// @brief Structure for initialize request parameters
struct InitializeParams {
  rfl::Rename<"protocolVersion", std::string> protocol_version;
  rfl::Rename<"capabilities", ClientCapabilities> capabilities;
  rfl::Rename<"clientInfo", Implementation> client_info;
};

/* --------------- server capabilities --------------- */

/// @brief Structure for prompts capabilities
struct PromptsCapabilities {
  rfl::Rename<"listChanged", std::optional<bool> > list_changed;
};

/// @brief Structure for resources capabilities
struct ResourcesCapabilities {
  rfl::Rename<"subscribe", std::optional<bool> > subscribe;
  rfl::Rename<"listChanged", std::optional<bool> > list_changed;
};

/// @brief Structure for tools capabilities
struct ToolsCapabilities {
  rfl::Rename<"listChanged", std::optional<bool> > list_changed;
};

/// @brief Structure representing server capabilities
struct ServerCapabilities {
  rfl::Rename<"experimental", OptionalParams> experimental;
  rfl::Rename<"logging", std::optional<rfl::Generic> > logging;
  rfl::Rename<"prompts", std::optional<PromptsCapabilities> > prompts;
  rfl::Rename<"resources", std::optional<ResourcesCapabilities> > resources;
  rfl::Rename<"tools", std::optional<ToolsCapabilities> > tools;
};

/// @brief Structure representing the result of an initialize request
struct InitializeResult {
  rfl::Rename<"protocolVersion", std::string> protocol_version;
  rfl::Rename<"capabilities", ServerCapabilities> capabilities;
  rfl::Rename<"serverInfo", Implementation> server_info;
  rfl::Rename<"instruction", std::optional<std::string> > instruction;
};

struct InitializeResultRPC {
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  rfl::Rename<"id", RequestId> id;
  rfl::Rename<"result", InitializeResult> result;
};

/* --------------- pagination / tools / resources / content --------------- */

/// @brief Structure containing parameters for paginated requests
struct PaginatedRequestParams {
  rfl::Rename<"cursor", std::optional<Cursor> > cursor;
};

/// @brief Structure representing a paginated request
struct PaginatedRequest {
  rfl::Rename<"params", std::optional<PaginatedRequestParams> > params;
};

/// @brief Structure representing an input schema for tool parameters
struct InputSchema {
  rfl::Rename<"type", std::string> type = "object";
  rfl::Rename<"properties", OptionalParams> properties;
  rfl::Rename<"required", std::optional<std::vector<std::string> > > required;
};

/// @brief Structure representing a tool with name, description, and input schema
struct Tool {
  rfl::Rename<"name", std::string> name;
  rfl::Rename<"description", std::optional<std::string> > description;
  rfl::Rename<"inputSchema", InputSchema> input_schema;
};

/// @brief Structure representing the result of a list tools request
struct ListToolsResult {
  rfl::Rename<"tools", std::vector<Tool> > tools;
};

/// @brief Structure representing text content
struct TextContent {
  rfl::Rename<"type", std::string> type = "text";
  rfl::Rename<"text", std::string> text;
};

/// @brief Structure representing image content
struct ImageContent {
  rfl::Rename<"type", std::string> type = "image";
  rfl::Rename<"data", std::string> data;
  rfl::Rename<"mimeType", std::string> mime_type;
};

/// @brief Structure representing a resource content with URI and optional MIME type
struct ResourceContent {
  rfl::Rename<"uri", std::string> uri;
  rfl::Rename<"mimeType", std::optional<std::string> > mime_type;
};

/// @brief Structure representing text resource content
struct TextResourceContent {
  rfl::Flatten<ResourceContent> flatten;
  rfl::Rename<"text", std::string> text;
};

/// @brief Structure representing blob resource content
struct BlobResourceContent {
  rfl::Flatten<ResourceContent> flatten;
  rfl::Rename<"blob", std::string> blob;
};

/// @brief Structure representing an embedded resource
struct EmbeddedResource {
  rfl::Rename<"type", std::string> type = "resource";
  rfl::Rename<"resource", std::variant<
                TextResourceContent, BlobResourceContent> > resource;
};

/// @brief Structure representing the result of a tool call
using VariantContent = std::variant<
  TextContent, ImageContent, EmbeddedResource>;

struct CallToolResult {
  rfl::Rename<"content", std::vector<VariantContent> > content;
  rfl::Rename<"isError", std::optional<bool> > is_error;
};

/// @brief Structure containing parameters for a tool call
struct CallToolParams {
  rfl::Rename<"name", std::string> name;
  rfl::Rename<"arguments", OptionalParams> arguments;
};

struct PropertySchema {
  std::string type;
};

struct ToolInputDef {
  std::string type;
  std::map<std::string, PropertySchema> properties;
  std::vector<std::string> required;
};

struct ToolInputSchema {
  rfl::Rename<"$schema",std::string> schema;
  rfl::Rename<"$ref", std::string> ref;
  rfl::Rename<"$defs", std::map<std::string, InputSchema>> defs;
};

} // namespace pxm::msg::types