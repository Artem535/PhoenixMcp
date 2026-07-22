include_guard(GLOBAL)

# Core dependencies are the complete dependency set of a stdio-only build.
find_package(reflectcpp CONFIG REQUIRED)
find_package(folly CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
set(PHOENIX_MCP_CORE_DEPENDENCIES reflectcpp::reflectcpp Folly::folly spdlog::spdlog)

if(PHOENIX_MCP_ENABLE_HTTP_CLIENT)
  find_package(OpenSSL CONFIG REQUIRED)
  set(PHOENIX_MCP_HTTP_CLIENT_DEPENDENCIES OpenSSL::SSL OpenSSL::Crypto)
endif()

if(PHOENIX_MCP_ENABLE_DROGON)
  find_package(Drogon CONFIG REQUIRED)
  set(PHOENIX_MCP_DROGON_DEPENDENCIES Drogon::Drogon)
endif()

if(PHOENIX_MCP_ENABLE_CROW)
  find_package(Crow CONFIG REQUIRED)
  set(PHOENIX_MCP_CROW_DEPENDENCIES Crow::Crow)
endif()

if(PHOENIX_MCP_ENABLE_OTEL)
  find_package(opentelemetry-cpp CONFIG REQUIRED)
  set(PHOENIX_MCP_OTEL_DEPENDENCIES opentelemetry-cpp::api)
endif()

if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)
endif()
