if(NOT DEFINED PHOENIX_MCP_SOURCE_DIR)
  message(FATAL_ERROR "PHOENIX_MCP_SOURCE_DIR is required")
endif()

file(READ "${PHOENIX_MCP_SOURCE_DIR}/CMakeLists.txt" phoenix_mcp_root)
file(READ "${PHOENIX_MCP_SOURCE_DIR}/VERSION" phoenix_mcp_version)

if(NOT phoenix_mcp_root MATCHES "cmake_minimum_required\\(VERSION 3\\.28\\)")
  message(FATAL_ERROR "The root CMake build must require CMake 3.28 or newer")
endif()

if(NOT phoenix_mcp_root MATCHES "Hand-maintained, authoritative CMake build")
  message(FATAL_ERROR "The root CMake build must declare its manual authority")
endif()

string(STRIP "${phoenix_mcp_version}" phoenix_mcp_version)
if(NOT phoenix_mcp_version STREQUAL "0.2.0")
  message(FATAL_ERROR "VERSION must contain the foundation version 0.2.0")
endif()
