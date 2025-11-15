set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)

set(INSTALL_GTEST OFF)

include(FetchContent)

if(NOT CMAKE_HOST_SYSTEM_NAME MATCHES "Windows" AND MSVC)
  set(gtest_disable_pthreads
      ON
      CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2)

FetchContent_MakeAvailable(googletest)

function(add_ai_cli_test test_name test_files)
  add_executable(${test_name} ${test_files})
  target_include_directories(
    ${test_name}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/json/include)
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
  target_link_libraries(${test_name} PRIVATE GTest::gtest GTest::gtest_main)
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_ai_cli_test(
  test_response
  "test_response.cc;test_stream_response.cc;test_tool_calls_stream_response.cc;../src/response.cpp"
)
