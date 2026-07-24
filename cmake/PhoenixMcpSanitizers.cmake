include_guard(GLOBAL)

# Sanitizer flags are applied PUBLIC, not PRIVATE: a static library compiled
# with -fsanitize=... still needs every executable that links it to pass the
# same flag at ITS OWN link step, or the link fails with undefined references
# to the sanitizer runtime (__asan_*/__ubsan_*). PUBLIC makes
# target_link_libraries(consumer ... phoenix_mcp_implementation) propagate the
# flags to every consumer automatically, instead of requiring every
# executable and test target to call this function on itself too.
function(phoenix_mcp_enable_sanitizers target)
  if(PHOENIX_MCP_ENABLE_THREAD_SANITIZER)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      target_compile_options(${target} PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
      target_link_options(${target} PUBLIC -fsanitize=thread)
    else()
      message(WARNING "ThreadSanitizer is not configured for ${CMAKE_CXX_COMPILER_ID}")
    endif()
    return()
  endif()

  if(NOT PHOENIX_MCP_ENABLE_SANITIZERS)
    return()
  endif()

  if(MSVC)
    target_compile_options(${target} PUBLIC /fsanitize=address)
    target_link_options(${target} PUBLIC /fsanitize=address)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PUBLIC -fsanitize=address,undefined
                           -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  else()
    message(WARNING "Sanitizers are not configured for ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()