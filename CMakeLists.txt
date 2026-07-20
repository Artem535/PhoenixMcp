# Hand-maintained, authoritative CMake build for PhoenixMcp.
cmake_minimum_required(VERSION 3.28)

if(NOT CMAKE_GENERATOR MATCHES "Ninja")
  message(FATAL_ERROR "PhoenixMcp requires the Ninja generator")
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PHOENIX_MCP_VERSION)
string(STRIP "${PHOENIX_MCP_VERSION}" PHOENIX_MCP_VERSION)
project(PhoenixMcp VERSION "${PHOENIX_MCP_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PhoenixMcpOptions)
include(PhoenixMcpDependencies)
include(PhoenixMcpWarnings)
include(PhoenixMcpSanitizers)

set(BUILD_TESTING ${PHOENIX_MCP_BUILD_TESTS} CACHE BOOL
    "Build the CTest test suite" FORCE)
include(CTest)
include(CPack)

if(PHOENIX_MCP_ENABLE_CLANG_TIDY)
  find_program(PHOENIX_MCP_CLANG_TIDY NAMES clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${PHOENIX_MCP_CLANG_TIDY}")
endif()

set(PHOENIX_MCP_LIBRARY_TYPE STATIC)
if(PHOENIX_MCP_BUILD_SHARED)
  set(PHOENIX_MCP_LIBRARY_TYPE SHARED)
endif()

set(phoenix_mcp_sources
    src/phoenix_mcp/runtime/runtime.cpp
    src/phoenix_mcp/server/mcp_request_handler.cpp
    src/phoenix_mcp/server/mcp_session.cpp
    src/phoenix_mcp/server/server.cpp
    src/phoenix_mcp/tool_registry/tool_registry.cpp)

if(PHOENIX_MCP_ENABLE_STDIO)
  list(APPEND phoenix_mcp_sources src/phoenix_mcp/transport/stdio_transport.cpp)
endif()

if(PHOENIX_MCP_ENABLE_DROGON)
  list(APPEND phoenix_mcp_sources src/phoenix_mcp/transport/drogon_transport.cpp)
endif()

if(PHOENIX_MCP_ENABLE_CROW)
  list(APPEND phoenix_mcp_sources src/phoenix_mcp/transport/crow_transport.cpp)
endif()

add_library(phoenix_mcp_implementation ${PHOENIX_MCP_LIBRARY_TYPE}
            ${phoenix_mcp_sources})
target_include_directories(phoenix_mcp_implementation
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
target_compile_definitions(phoenix_mcp_implementation
  PRIVATE GLOG_USE_GLOG_EXPORT
  PUBLIC PXM_WITH_OTEL=$<BOOL:${PHOENIX_MCP_ENABLE_OTEL}>)
target_link_libraries(phoenix_mcp_implementation
  PUBLIC ${PHOENIX_MCP_CORE_DEPENDENCIES})

if(PHOENIX_MCP_ENABLE_DROGON)
  target_link_libraries(phoenix_mcp_implementation
                        PRIVATE ${PHOENIX_MCP_DROGON_DEPENDENCIES})
endif()

if(PHOENIX_MCP_ENABLE_CROW)
  target_link_libraries(phoenix_mcp_implementation
                        PRIVATE ${PHOENIX_MCP_CROW_DEPENDENCIES})
endif()

if(PHOENIX_MCP_ENABLE_OTEL)
  target_link_libraries(phoenix_mcp_implementation
                        PRIVATE ${PHOENIX_MCP_OTEL_DEPENDENCIES})
endif()

phoenix_mcp_enable_warnings(phoenix_mcp_implementation)
phoenix_mcp_enable_sanitizers(phoenix_mcp_implementation)

function(phoenix_mcp_add_module name)
  add_library(phoenix_mcp_${name} INTERFACE)
  add_library(PhoenixMcp::${name} ALIAS phoenix_mcp_${name})
endfunction()

# The current server implementation is private.  These interfaces preserve the
# documented acyclic public module graph while the source tree is migrated.
phoenix_mcp_add_module(Core)
target_link_libraries(phoenix_mcp_Core INTERFACE phoenix_mcp_implementation)

phoenix_mcp_add_module(Protocol)
target_link_libraries(phoenix_mcp_Protocol INTERFACE PhoenixMcp::Core)

phoenix_mcp_add_module(Server)
target_link_libraries(phoenix_mcp_Server INTERFACE PhoenixMcp::Protocol)

phoenix_mcp_add_module(Client)
target_link_libraries(phoenix_mcp_Client INTERFACE PhoenixMcp::Protocol)

phoenix_mcp_add_module(Host)
target_link_libraries(phoenix_mcp_Host INTERFACE PhoenixMcp::Client)

phoenix_mcp_add_module(TransportStdio)
target_link_libraries(phoenix_mcp_TransportStdio INTERFACE PhoenixMcp::Core)

phoenix_mcp_add_module(TransportHttp)
target_link_libraries(phoenix_mcp_TransportHttp INTERFACE PhoenixMcp::Core)

phoenix_mcp_add_module(TransportDrogon)
target_link_libraries(phoenix_mcp_TransportDrogon INTERFACE PhoenixMcp::TransportHttp)

phoenix_mcp_add_module(TransportCrow)
target_link_libraries(phoenix_mcp_TransportCrow INTERFACE PhoenixMcp::TransportHttp)

phoenix_mcp_add_module(Telemetry)
target_link_libraries(phoenix_mcp_Telemetry INTERFACE PhoenixMcp::Core)

phoenix_mcp_add_module(Testing)
target_link_libraries(phoenix_mcp_Testing INTERFACE PhoenixMcp::Core)

if(PHOENIX_MCP_BUILD_EXAMPLES)
  if(PHOENIX_MCP_ENABLE_STDIO)
    add_executable(create_server examples/create_server/main.cpp)
    target_link_libraries(create_server PRIVATE PhoenixMcp::Server
                                                PhoenixMcp::TransportStdio)
  endif()

  add_executable(using_tool_registry examples/using_tool_registry/main.cpp)
  target_link_libraries(using_tool_registry PRIVATE PhoenixMcp::Server)

  if(PHOENIX_MCP_ENABLE_DROGON)
    add_executable(create_server_http examples/create_server_http/main.cpp)
    target_link_libraries(create_server_http PRIVATE PhoenixMcp::Server
                                                     PhoenixMcp::TransportDrogon)
  endif()
endif()

if(BUILD_TESTING)
  add_test(
    NAME phoenix_mcp_manual_authority
    COMMAND ${CMAKE_COMMAND}
      -DPHOENIX_MCP_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/verify_manual_authority.cmake)
endif()
