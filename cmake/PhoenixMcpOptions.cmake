include_guard(GLOBAL)

# Local development defaults build the library's checks and examples.
option(PHOENIX_MCP_BUILD_TESTS "Build PhoenixMcp tests" ON)
option(PHOENIX_MCP_BUILD_EXAMPLES "Build PhoenixMcp examples" ON)
option(PHOENIX_MCP_BUILD_BENCHMARKS "Build PhoenixMcp benchmarks" OFF)
option(PHOENIX_MCP_BUILD_SHARED "Build PhoenixMcp as shared libraries" OFF)

# Transport and integration defaults keep the core stdio-only.
option(PHOENIX_MCP_ENABLE_STDIO "Build the stdio transport" ON)
option(PHOENIX_MCP_ENABLE_HTTP_CLIENT "Enable HTTP client support" OFF)
option(PHOENIX_MCP_ENABLE_DROGON "Build the Drogon transport adapter" OFF)
option(PHOENIX_MCP_ENABLE_CROW "Build the Crow transport adapter" OFF)
option(PHOENIX_MCP_ENABLE_OTEL "Enable OpenTelemetry integration" OFF)

# Tooling is opt-in so default release builds stay portable.
option(PHOENIX_MCP_ENABLE_SANITIZERS "Enable address and undefined sanitizers" OFF)
option(PHOENIX_MCP_ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)
option(PHOENIX_MCP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
