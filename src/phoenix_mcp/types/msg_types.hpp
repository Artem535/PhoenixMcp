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

/* ---------- Basic Types ---------- */

/// @brief Request identifier used for correlating JSON-RPC requests and responses
/// @details Can be either integer or string to accommodate different client implementations
using RequestId = std::variant<int, std::string>;

/// @brief Container for optional method parameters
/// @details Uses rfl::Generic to represent flexible JSON-like parameter objects
using OptionalParams = std::optional<rfl::Generic>;

/// @brief Generic result type for successful responses
/// @details Provides flexibility in returning various data structures from methods
using Result = rfl::Generic;

/// @brief Structure representing an empty result
/// @details Standard representation for operations that don't return meaningful data
using EmptyResult = std::map<std::string, rfl::Generic>;

/// @brief Token used to track progress of long-running operations
/// @details Can be either integer or string identifier for progress reporting
using ProgressToken = std::variant<int, std::string>;

/// @brief Cursor for pagination through large result sets
/// @details String-based pointer used to navigate between pages of results
using Cursor = std::string;

/// @brief Specification of logging verbosity level
/// @details String value representing standard log levels: "debug", "info", "warning", "error"
using LoggingLevel = std::string;

/// @brief Identifier for participant role in conversation
/// @details String value indicating role: "user", "assistant", or "system"
using Role = std::string;

/* ---------- JSON-RPC Structures ---------- */

/// @brief Basic JSON-RPC request structure
/// @details Minimal structure containing essential fields for a JSON-RPC request
struct Request {
  std::string jsonrpc = "2.0"; /// @brief JSON-RPC protocol version identifier
  std::string method; /// @brief Name of the method to be invoked
  RequestId id; /// @brief Unique identifier for request-response correlation
  OptionalParams params; /// @brief Optional parameters for the requested method

};

/// @brief Extended JSON-RPC request with optional parameters
/// @details Complete request structure that includes parameters for method invocation
struct MinimalRequest {
  std::string jsonrpc = "2.0"; /// @brief JSON-RPC protocol version identifier
  std::string method; /// @brief Name of the method to be invoked
  RequestId id; /// @brief Unique identifier for request-response correlation
};

/// @brief JSON-RPC response structure for successful operations
/// @details Contains the result of a successfully processed request
struct Response {
  std::string jsonrpc = "2.0"; /// @brief JSON-RPC protocol version identifier
  Result result; /// @brief Result data from the processed request
  RequestId id; /// @brief Identifier of the corresponding request
};

/// @brief Notification message structure
/// @details One-way message from client to server that doesn't require a response
struct Notification {
  std::string jsonrpc = "2.0"; /// @brief JSON-RPC protocol version identifier
  std::string method; /// @brief Name of the method to be invoked
  OptionalParams params; /// @brief Optional parameters for the notification
};

/// @brief Container for error information
/// @details Holds detailed information about errors that occur during request processing
struct ErrorData {
  int code; /// @brief Numeric error code following JSON-RPC standards
  std::string message; /// @brief Human-readable description of the error
  rfl::Generic data; /// @brief Additional context-specific error details
};

/// @brief Complete error response structure
/// @details Response sent when a request cannot be successfully processed
struct Error {
  std::string jsonrpc = "2.0"; /// @brief JSON-RPC protocol version identifier
  RequestId id; /// @brief Identifier of the failed request
  ErrorData error; /// @brief Detailed error information
};

/* ---------- Pagination / Content / Tools ---------- */

/// @brief Parameters for paginated requests
/// @details Controls how paginated data retrieval is performed
struct PaginatedRequestParams {
  std::optional<Cursor> cursor;
  /// @brief Optional cursor for resuming pagination
};

/// @brief Wrapper for requests that support pagination
/// @details Provides a standardized way to handle paginated operations
struct PaginatedRequest {
  /// @brief Optional pagination control parameters
  std::optional<PaginatedRequestParams> params;
};

/// @brief Schema definition for tool input parameters
/// @details Defines the structure and validation rules for tool inputs
struct InputSchema {
  std::string type = "object"; /// @brief JSON schema type (fixed to "object")
  OptionalParams properties;
  /// @brief Definition of object properties
   /// @brief List of property names that are required
  std::optional<std::vector<std::string> > required;
};

/// @brief Definition of an available tool
/// @details Contains metadata and input schema for a callable tool
struct Tool {
  std::string name;
  /// @brief Unique identifier for the tool
   /// @brief Optional human-readable description of tool functionality
  std::optional<std::string> description;
  /// @brief Schema defining valid input parameters for the tool
  rfl::Rename<"inputSchema", InputSchema> input_schema;
};

/// @brief Result structure for listing available tools
/// @details Contains the complete list of tools exposed by the server
struct ListToolsResult {
  std::vector<Tool> tools; /// @brief Collection of available tools
};

/* --- Content types --- */

/// @brief Text content representation
/// @details Structure for plain text content
struct TextContent {
  std::string type = "text"; /// @brief Content type discriminator
  std::string text; /// @brief Actual text content
};

/// @brief Image content representation
/// @details Structure for image data
struct ImageContent {
  std::string type = "image"; /// @brief Content type discriminator
  std::string data;
  /// @brief Image data (typically base64 encoded)
   /// @brief MIME type of the image (e.g., "image/png")
  rfl::Rename<"mimeType", std::string> mime_type;
};

/// @brief Generic resource reference
/// @details Points to an external resource by URI
struct ResourceContent {
  std::string uri;
  /// @brief Uniform Resource Identifier
   /// @brief Optional MIME type of the referenced resource
  rfl::Rename<"mimeType", std::optional<std::string> > mime_type;
};

/// @brief Text resource content
/// @details Resource that contains text data
struct TextResourceContent {
  /// @brief Inherited resource metadata
  rfl::Flatten<ResourceContent> flatten;
  std::string text; /// @brief Text content of the resource
};

/// @brief Binary resource content
/// @details Resource that contains binary/blob data
struct BlobResourceContent {
  /// @brief Inherited resource metadata
  rfl::Flatten<ResourceContent> flatten;
  std::string blob; /// @brief Binary content of the resource
};

/// @brief Embedded resource reference
/// @details Represents a resource embedded within content
struct EmbeddedResource {
  std::string type = "resource";
  /// @brief Content type discriminator
   /// @brief Actual resource content (text or binary)
  std::variant<TextResourceContent, BlobResourceContent> resource;
};

/// @brief Variant type for different content types
/// @details Can hold any of the supported content structures
using VariantContent = std::variant<TextContent, ImageContent, EmbeddedResource>
;

/// @brief Result of a tool execution
/// @details Contains output content from tool execution
struct CallToolResult {
  /// @brief Output content produced by the tool
  std::vector<VariantContent> content;
  /// @brief Optional flag indicating if result represents an error
  rfl::Rename<"isError", std::optional<bool> > is_error;
};

/* ---------- Tool Calls ---------- */

/// @brief Parameters for calling a tool
/// @details Specifies which tool to call and with what arguments
struct CallToolParams {
  std::string name; /// @brief Name of the tool to invoke
  OptionalParams arguments; /// @brief Arguments to pass to the tool
};

/// @brief JSON schema property definition
/// @details Defines properties within a JSON schema
struct PropertySchema {
  std::string type; /// @brief Data type of the property
};

/// @brief Complete tool input schema with references
/// @details Extended schema definition supporting JSON schema references
struct ToolInputSchema {
  /// @brief JSON schema version identifier
  rfl::Rename<"$schema", std::string> schema;
  /// @brief Reference to another schema definition
  rfl::Rename<"$ref", std::string> ref;
  /// @brief Local schema definitions for reuse
  rfl::Rename<"$defs", std::map<std::string, InputSchema> > defs;
};

/* ---------- Requests ---------- */

/// @brief Initialization request from client
/// @details First message sent by client to establish connection
struct InitializeRequest {
  /// @brief Inherited JSON-RPC request fields
  rfl::Flatten<Request> flatten;
  /// @brief Method name constant
  static constexpr std::string_view method = "initialize";
};

/// @brief Ping request for connectivity testing
/// @details Used to check server availability and responsiveness
struct PingRequest {
  /// @brief Inherited JSON-RPC request fields
  rfl::Flatten<Request> flatten;
  /// @brief Optional parameters for ping operation
  OptionalParams params;
  /// @brief Method name constant
  static constexpr std::string_view method = "ping";
};

/// @brief Request to list available tools
/// @details Asks server to return all registered tools
struct ListToolsRequest {
  /// @brief Inherited JSON-RPC request fields
  rfl::Flatten<Request> flatten;
  /// @brief Method name constant
  static constexpr std::string_view method = "tools/list";
};

/// @brief Request to execute a specific tool
/// @details Invokes a tool with specified arguments
struct CallToolRequest {
  /// @brief Inherited JSON-RPC request fields
  rfl::Flatten<MinimalRequest> flatten;
  /// @brief Method name constant
  static constexpr std::string_view method = "tools/call";
  /// @brief Optional parameters specifying tool and arguments
  std::optional<CallToolParams> params;
};

/* ---------- Notifications ---------- */

/// @brief Cancellation notification
/// @details Informs server that a previous request has been cancelled
struct CancelNotification {
  /// @brief Inherited notification fields
  rfl::Flatten<Notification> flatten;
  /// @brief Method name constant
  static constexpr std::string_view method = "notifications/cancelled";
};

/// @brief Initialization complete notification
/// @details Sent by server after successful initialization
struct InitializeNotification {
  rfl::Flatten<Notification> flatten;
  /// @brief Inherited notification fields
  /// @brief Method name constant
  static constexpr std::string_view method = "notifications/initialized";
};

/// @brief Tools list changed notification
/// @details Informs client that available tools have changed
struct ToolListChangedNotification {
  /// @brief Inherited notification fields
  rfl::Flatten<Notification> flatten;
  /// @brief Method name constant
  static constexpr std::string_view method = "notification/tools/listChanged";
};

/* ---------- Capability Parameters ---------- */

/// @brief Parameters for cancellation notifications
/// @details Provides context for why an operation was cancelled
struct NotificationParams {
  RequestId request_id; /// @brief ID of the cancelled request
  std::optional<std::string> reason; /// @brief Optional reason for cancellation
};

/// @brief Parameters for roots capability
/// @details Configuration for root directory monitoring
struct RootsParams {
  std::optional<bool> list_changed; /// @brief Whether list of roots can change
};

/// @brief Client capabilities declaration
/// @details What features the client supports
struct ClientCapabilities {
  OptionalParams experimental; /// @brief Experimental features support
  std::optional<RootsParams> roots; /// @brief Roots monitoring capability
  std::optional<rfl::Generic> sampling; /// @brief Sampling capability
};

/// @brief Server implementation metadata
/// @details Information about server software
struct Implementation {
  std::string name; /// @brief Server name
  std::string version; /// @brief Server version
};

/// @brief Parameters for initialization request
/// @details Contains client capabilities and metadata for setup
struct InitializeParams {
  /// @brief MCP protocol version supported by client
  rfl::Rename<"protocolVersion", std::string> protocol_version;

  /// @brief Features supported by the client
  ClientCapabilities capabilities;

  /// @brief Information about the connecting client
  rfl::Rename<"clientInfo", Implementation> client_info;
};

/* ---------- Server Capabilities ---------- */

/// @brief Prompts-related server capabilities
/// @details What prompt-related features the server supports
struct PromptsCapabilities {
  /// @brief Whether prompt list can change dynamically
  rfl::Rename<"listChanged", std::optional<bool> > list_changed;
};

/// @brief Resources-related server capabilities
/// @details What resource-related features the server supports
struct ResourcesCapabilities {
  /// @brief Whether clients can subscribe to resource changes
  rfl::Rename<"subscribe", std::optional<bool> > subscribe;
  /// @brief Whether resource list can change dynamically
  rfl::Rename<"listChanged", std::optional<bool> > list_changed;
};

/// @brief Tools-related server capabilities
/// @details What tool-related features the server supports
struct ToolsCapabilities {
  /// @brief Whether tool list can change dynamically
  rfl::Rename<"listChanged", std::optional<bool> > list_changed;
};

/// @brief Complete server capabilities declaration
/// @details All features supported by the server
struct ServerCapabilities {
  OptionalParams experimental; /// @brief Experimental features
  std::optional<rfl::Generic> logging; /// @brief Logging capability
  std::optional<PromptsCapabilities> prompts; /// @brief Prompts capability
  std::optional<ResourcesCapabilities> resources;
  /// @brief Resources capability
  std::optional<ToolsCapabilities> tools; /// @brief Tools capability
};

/// @brief Result of successful initialization
/// @details Server capabilities and metadata sent to client
struct InitializeResult {
  /// @brief MCP protocol version supported by server
  rfl::Rename<"protocolVersion", std::string> protocol_version;

  ServerCapabilities capabilities;
  /// @brief Features supported by this server

  /// @brief Information about this server instance
  rfl::Rename<"serverInfo", Implementation> server_info;

  std::optional<std::string> instruction;
  /// @brief Optional instruction for client behavior
};

/// @brief JSON-RPC wrapper for initialization result
/// @details Complete response structure for initialize request
struct InitializeResultRPC {
  /// @brief JSON-RPC protocol version
  std::string jsonrpc = "2.0";
  /// @brief ID of the original initialize request
  RequestId id;
  /// @brief Actual initialization result data
  InitializeResult result;
};

} // namespace pxm::msg::types