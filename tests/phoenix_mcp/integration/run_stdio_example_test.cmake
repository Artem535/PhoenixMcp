if(NOT DEFINED SERVER_EXECUTABLE)
  message(FATAL_ERROR "SERVER_EXECUTABLE is required")
endif()

if(NOT EXISTS "${SERVER_EXECUTABLE}")
  message(FATAL_ERROR "Stdio example does not exist: ${SERVER_EXECUTABLE}")
endif()

set(input_path "${CMAKE_CURRENT_BINARY_DIR}/phoenix_mcp_stdio_example_input.jsonl")
file(WRITE "${input_path}" [=[{"jsonrpc":"2.0","id":1,"method":"initialize"}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":2,"method":"ping"}
]=])

execute_process(
  COMMAND "${SERVER_EXECUTABLE}"
  INPUT_FILE "${input_path}"
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error_output
  RESULT_VARIABLE result
  TIMEOUT 10)

if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "Stdio example exited with ${result}. stderr: ${error_output}")
endif()

string(REGEX MATCH "\\\"id\\\":1" initialize_response "${output}")
if(NOT initialize_response)
  message(FATAL_ERROR "Missing initialize response: ${output}")
endif()

string(REGEX MATCH "\\\"id\\\":2" ping_response "${output}")
if(NOT ping_response)
  message(FATAL_ERROR "Missing ping response: ${output}")
endif()
