# ── Test configuration ──────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)

include(FetchContent)

# Disable pthreads in gtest on non-Windows non-MSVC platforms
if(NOT CMAKE_HOST_SYSTEM_NAME MATCHES "Windows" AND NOT MSVC)
  set(gtest_disable_pthreads
      ON
      CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2)

FetchContent_MakeAvailable(googletest)

# Suppress -Wcharacter-conversion for gtest/gmock when using Clang and C++20.
# char8_t (C++20) to char32_t implicit conversion in gtest-printers.h triggers
# this warning, which becomes an error under -WX.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  foreach(gtarget gtest gtest_main gmock gmock_main)
    if(TARGET ${gtarget})
      target_compile_options(${gtarget} PRIVATE -Wno-character-conversion)
    endif()
  endforeach()
endif()

# ── Helper function to add a test executable ───────────────────────────────
function(add_ai_cli_test test_name test_files)
  add_executable(${test_name} ${test_files} main.cc $<TARGET_OBJECTS:ai_core>)
  target_include_directories(
    ${test_name}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/json/include
            ${CMAKE_CURRENT_BINARY_DIR}/..)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
     OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
     OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    if(NOT MSVC)
      target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    endif()
  endif()

  if(MSVC)
    target_compile_options(${test_name} PRIVATE /W4 /WX /utf-8 /EHsc)
  endif()

  target_link_libraries(${test_name} PRIVATE GTest::gtest ai_core)
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

file(GLOB_RECURSE test_files "test_*.cc" "*_test.cc")

# Exclude test_stream_response.cc on Windows MSVC (non-clang) builds:
# tests/stream_response_data.h defines a very long string literal that causes
# the MSVC cl.exe command line to be truncated, leading to compilation failure.
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(REMOVE_ITEM test_files
       "${CMAKE_CURRENT_SOURCE_DIR}/test_stream_response.cc"
       "${CMAKE_CURRENT_SOURCE_DIR}/test_tool_calls_stream_response.cc")
endif()

foreach(test_file ${test_files})
  get_filename_component(test_name ${test_file} NAME_WE)
  add_ai_cli_test(${test_name} ${test_file})
endforeach()

add_ai_cli_test(all_test "${test_files}")
