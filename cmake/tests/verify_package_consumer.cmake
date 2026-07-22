cmake_minimum_required(VERSION 3.28)

set(consumer_dir "${CMAKE_CURRENT_BINARY_DIR}/test_package_consumer")

file(REMOVE_RECURSE "${consumer_dir}")
file(MAKE_DIRECTORY "${consumer_dir}")

execute_process(
  COMMAND ${CMAKE_COMMAND} --install "${BUILD_DIR}"
    --prefix "${INSTALL_PREFIX}"
  RESULT_VARIABLE install_result
  OUTPUT_VARIABLE install_output
  ERROR_VARIABLE install_error)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "Install failed:\n${install_output}\n${install_error}")
endif()

file(WRITE "${consumer_dir}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.28)\n"
  "project(PhoenixMcpConsumerTest LANGUAGES CXX)\n"
  "set(CMAKE_CXX_STANDARD 20)\n"
  "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
  "set(CMAKE_CXX_EXTENSIONS OFF)\n"
  "find_package(PhoenixMcp CONFIG REQUIRED PATHS \"${INSTALL_PREFIX}\")\n"
  "add_executable(consumer_check main.cpp)\n"
  "target_link_libraries(consumer_check PRIVATE PhoenixMcp::Server PhoenixMcp::TransportStdio)\n")

file(WRITE "${consumer_dir}/main.cpp"
  "#include \"phoenix_mcp/protocol/message_types.h\"\n"
  "#include \"phoenix_mcp/server/server.h\"\n"
  "#include \"phoenix_mcp/transport/stdio_transport.h\"\n"
  "int main() { return 0; }\n")

# The vcpkg triplet root contains share/<pkg>/ for all installed packages.
set(vcpkg_root "${BUILD_DIR}/vcpkg_installed/x64-linux")

execute_process(
  COMMAND ${CMAKE_COMMAND} -G Ninja -S "${consumer_dir}" -B "${consumer_dir}/build"
    -DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}\;${vcpkg_root}
  RESULT_VARIABLE config_result
  OUTPUT_VARIABLE config_output
  ERROR_VARIABLE config_error)
if(NOT config_result EQUAL 0)
  message(FATAL_ERROR "Consumer configure failed:\n${config_output}\n${config_error}")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} --build "${consumer_dir}/build"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Consumer build failed:\n${build_output}\n${build_error}")
endif()

message(STATUS "Package consumer test passed: ${consumer_dir}")