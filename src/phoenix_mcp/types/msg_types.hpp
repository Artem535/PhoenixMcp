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
using OptionalParams = std::optional<std::map<std::string, rfl::Generic> >;

/// @brief Type alias for result map with generic values
using Result = std::map<std::string, rfl::Generic>;

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

/// @brief Structure representing a JSON-RPC request
struct Request {
  /// @brief JSON-RPC protocol version, defaults to "2.0"
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  /// @brief The method name to be invoked
  rfl::Rename<"method", std::string> method;
  /// @brief The request identifier
  rfl::Rename<"id", RequestId> id;
  /// @brief Optional parameters for the method
  rfl::Rename<"params", OptionalParams> params;
};


/// @brief Structure representing a JSON-RPC response
struct Response {
  /// @brief JSON-RPC protocol version, defaults to "2.0"
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  /// @brief The result of the method call
  rfl::Rename<"result", Result> result;
  /// @brief The request identifier
  rfl::Rename<"id", RequestId> id;
};

/// @brief Structure representing a JSON-RPC notification
struct Notification {
  /// @brief JSON-RPC protocol version, defaults to "2.0"
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  /// @brief Optional parameters for the notification
  rfl::Rename<"params", OptionalParams> params;
};

/// @brief Structure containing error details
struct ErrorData {
  /// @brief Error code
  rfl::Rename<"code", int> code;
  /// @brief Error message
  rfl::Rename<"message", std::string> message;
  /// @brief Optional additional error data
  rfl::Rename<"data", rfl::Generic> data;
};

/// @brief Structure representing a JSON-RPC error response
struct Error {
  /// @brief JSON-RPC protocol version, defaults to "2.0"
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  /// @brief The request identifier (can be null for parse errors)
  rfl::Rename<"id", RequestId> id;
  /// @brief The error details
  rfl::Rename<"error", ErrorData> error;
};

/// @brief Structure for notification parameters containing request ID and optional reason
struct NotificationParams {
  /// @brief The request identifier this notification relates to
  rfl::Rename<"requestId", RequestId> params;
  /// @brief Optional reason for the notification
  rfl::Rename<"reason", std::optional<std::string> > reason;
};

/// @brief Structure representing a cancellation notification
struct CancelNotification {
  /// @brief The base notification structure
  rfl::Flatten<Notification> notification;
  /// @brief The cancellation method name
  rfl::Rename<"method", std::string> method = "notifications/cancelled";
};


/// @brief Structure containing roots capability parameters
struct RootsParams {
  /// @brief Indicates if the list has changed
  rfl::Rename<"listChanged", std::optional<bool> > listChanged;
};

/// @brief Structure representing client capabilities
struct ClientCapabilities {
  /// @brief Optional experimental capabilities
  rfl::Rename<"experimental", OptionalParams> experimental;
  /// @brief Optional roots capabilities
  rfl::Rename<"roots", std::optional<RootsParams> > roots;
  /// @brief Optional sampling capabilities
  rfl::Rename<"sampling", std::optional<rfl::Generic> > sampling;
};

/// @brief Structure representing implementation details (name and version)
struct Implementation {
  /// @brief Name of the implementation
  rfl::Rename<"name", std::string> name;
  /// @brief Version of the implementation
  rfl::Rename<"version", std::string> version;
};

/// @brief Structure for initialize request parameters
struct InitializeParams {
  /// @brief Protocol version being used
  rfl::Rename<"protocolVersion", std::string> protocolVersion;
  /// @brief Client capabilities
  rfl::Rename<"capabilities", ClientCapabilities> capabilities;
  /// @brief Information about the client implementation
  rfl::Rename<"clientInfo", Implementation> clientInfo;
};

/// @brief Structure representing an initialize request
struct InitializeRequest {
  /// @brief JSON-RPC protocol version, defaults to "2.0"
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  /// @brief The method name, defaults to "initialize"
  rfl::Rename<"method", std::string> method = "initialize";
  /// @brief The request identifier
  rfl::Rename<"id", RequestId> id;
  /// @brief Optional parameters for initialization
  rfl::Rename<"params", OptionalParams> params;
};


// -----------------------------------------------------------------------------
// Server Capabilities Structures
// -----------------------------------------------------------------------------

/// @brief Structure for prompts capabilities
struct PromptsCapabilities {
  /// @brief Indicates if the prompts list has changed
  rfl::Rename<"listChanged", std::optional<bool> > listChanged;
};

/// @brief Structure for resources capabilities
struct ResourcesCapabilities {
  /// @brief Indicates if resource subscription is supported
  rfl::Rename<"subscribe", std::optional<bool> > subscribe;
  /// @brief Indicates if the resources list has changed
  rfl::Rename<"listChanged", std::optional<bool> > listChanged;
};

/// @brief Structure for tools capabilities
struct ToolsCapabilities {
  /// @brief Indicates if the tools list has changed
  rfl::Rename<"listChanged", std::optional<bool> > listChanged;
};

/// @brief Structure representing server capabilities
struct ServerCapabilities {
  /// @brief Optional experimental capabilities
  rfl::Rename<"experimental", OptionalParams> experimental;
  /// @brief Optional logging capabilities
  rfl::Rename<"logging", std::optional<rfl::Generic> > logging;
  /// @brief Optional prompts capabilities
  rfl::Rename<"prompts", std::optional<PromptsCapabilities> > prompts;
  /// @brief Optional resources capabilities
  rfl::Rename<"resources", std::optional<ResourcesCapabilities> > resources;
  /// @brief Optional tools capabilities
  rfl::Rename<"tools", std::optional<ToolsCapabilities> > tools;
};


/// @brief Structure representing the result of an initialize request
struct InitializeResult {
  /// @brief The protocol version supported by the server
  rfl::Rename<"protocolVersion", std::string> protocolVersion;
  /// @brief The server capabilities
  rfl::Rename<"capabilities", ServerCapabilities> capabilities;
  /// @brief Information about the server implementation
  rfl::Rename<"serverInfo", Implementation> serverInfo;
  /// @brief Optional initialization instruction
  rfl::Rename<"instruction", std::optional<std::string> > instruction;
};

/// @brief Structure representing an initialization notification
struct InitializeNotification {
  /// @brief JSON-RPC protocol version, defaults to "2.0"
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  /// @brief The notification method name
  rfl::Rename<"method", std::string> method = "notification/initialized";
  /// @brief Optional notification parameters
  rfl::Rename<"params", OptionalParams> params;
};

/// @brief Structure representing a ping request
struct PingRequest {
  /// @brief JSON-RPC protocol version, defaults to "2.0"
  rfl::Rename<"jsonrpc", std::string> jsonrpc = "2.0";
  /// @brief The method name, defaults to "ping"
  rfl::Rename<"method", std::string> method = "ping";
  /// @brief The request identifier
  rfl::Rename<"id", RequestId> id;
  /// @brief Optional parameters for the ping request
  rfl::Rename<"params", OptionalParams> params;
};

/// @brief Structure containing parameters for paginated requests
struct PaginatedRequestParams {
  /// @brief Optional cursor for pagination
  rfl::Rename<"cursor", std::optional<Cursor> > cursor;
};

/// @brief Structure representing a paginated request
struct PaginatedRequest {
  /// @brief Optional pagination parameters
  rfl::Rename<"params", std::optional<PaginatedRequestParams> > params;
};

/// @brief Structure representing an input schema for tool parameters
struct InputSchema {
  /// @brief Schema type, always "object"
  rfl::Rename<"type", std::string> type = "object";
  /// @brief Schema properties map
  rfl::Rename<"properties", OptionalParams> properties;
  /// @brief Optional list of required property names
  rfl::Rename<"required", std::optional<std::vector<std::string> > > required;
};

/// @brief Structure representing a tool with name, description, and input schema
struct Tool {
  /// @brief The tool name
  rfl::Rename<"name", std::string> name;
  /// @brief Optional tool description
  rfl::Rename<"description", std::optional<std::string> > description;
  /// @brief The input schema for the tool
  rfl::Rename<"inputShema", InputSchema> inputSchema;
};

/// @brief Structure for requesting a list of tools
struct ListToolsRequest {
  /// @brief The method name, defaults to "tools/list"
  rfl::Rename<"method", std::string> method = "tools/list";
  /// @brief Optional pagination parameters
  rfl::Rename<"params", std::optional<PaginatedRequestParams> > params;
};

/// @brief Structure representing the result of a list tools request
struct ListToolsResult {
  /// @brief Vector of available tools
  rfl::Rename<"tools", std::vector<Tool> > tools;
};

/// @brief Structure representing text content
struct TextContent {
  /// @brief Content type, always "text"
  rfl::Rename<"type", std::string> type = "text";
  /// @brief The actual text content
  rfl::Rename<"text", std::string> text;
};

/// @brief Structure representing image content
struct ImageContent {
  /// @brief Content type, always "image"
  rfl::Rename<"type", std::string> type = "image";
  /// @brief Image data in base64 format
  rfl::Rename<"data", std::string> data;
  /// @brief MIME type of the image
  rfl::Rename<"mimeType", std::string> mimeType;
};


/// @brief Structure representing a resource content with URI and optional MIME type
struct ResourceContent {
  /// @brief URI of the resource
  rfl::Rename<"uri", std::string> uri;
  /// @brief Optional MIME type of the resource
  rfl::Rename<"mimeType", std::optional<std::string> > mimeType;
};

/// @brief Structure representing text resource content
struct TextResourceContent {
  /// @brief Base resource content structure
  rfl::Flatten<ResourceContent> flatten;
  /// @brief The text content of the resource
  rfl::Rename<"text", std::string> text;
};

/// @brief Structure representing blob resource content
struct BlobResourceContent {
  /// @brief Base resource content structure
  rfl::Flatten<ResourceContent> flatten;
  /// @brief The blob content of the resource in base64
  rfl::Rename<"blob", std::string> blob;
};


/// @brief Structure representing an embedded resource
struct EmbeddedResource {
  /// @brief Content type, always "resource"
  rfl::Rename<"type", std::string> type = "resource";
  /// @brief The embedded resource content (either text or blob)
  rfl::Rename<"resource", std::variant<
                TextResourceContent, BlobResourceContent> > embedded;
};

/// @brief Structure representing the result of a tool call
struct CallToolResult {
  /// @brief Vector of content returned by the tool
  rfl::Rename<"content", std::vector<std::variant<
                TextContent, ImageContent, EmbeddedResource> > > content;
  /// @brief Optional flag indicating if the tool call resulted in an error
  rfl::Rename<"isError", std::optional<bool> > isError;
};

/// @brief Structure containing parameters for a tool call
struct CallToolParams {
  /// @brief The name of the tool to call
  rfl::Rename<"name", std::string> name;
  /// @brief Arguments to pass to the tool
  rfl::Rename<"arguments", OptionalParams> arguments;
};

/// @brief Structure representing a tool call request
struct CallToolRequest {
  /// @brief The method name, defaults to "tools/call"
  rfl::Rename<"method", std::string> method = "tools/call";
  /// @brief Optional pagination parameters
  rfl::Rename<"params", std::optional<PaginatedRequestParams> > params;
};

/// @brief Structure representing a notification when the tool list changes
struct ToolListChangedNotification {
  /// @brief Base notification structure
  rfl::Flatten<Notification> flatten;
  /// @brief The notification method name
  rfl::Rename<"method", std::string> method = "notification/tools/listChanged";
};

}