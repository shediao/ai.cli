cmake_minimum_required(VERSION 3.20)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  cmake_policy(SET CMP0135 NEW)
endif()

# ── Project ─────────────────────────────────────────────────────────────────
project(
  ai_cli
  VERSION 0.1.0
  LANGUAGES CXX)

# ── Default build type ─────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Debug
      CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel..."
        FORCE)
endif()

# ── C++ standard ───────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT MSYS)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# ── Compiler warnings ──────────────────────────────────────────────────────
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /utf-8 /EHsc")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang" AND NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# ── Sanitizers (Debug mode on Linux/macOS) ───────────────────────────────
option(AICLI_ENABLE_ASAN "Enable Address/Undefined Sanitizers in Debug" ON)

if(AICLI_ENABLE_ASAN
   AND NOT CMAKE_CROSSCOMPILING
   AND CMAKE_HOST_SYSTEM_NAME MATCHES "Linux|Darwin")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls")
  endif()
endif()

# ── Testing ────────────────────────────────────────────────────────────────
option(AICLI_BUILD_TESTS "Build unit tests" ON)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(_default_tests ON)
else()
  set(_default_tests OFF)
endif()

# ── Options ────────────────────────────────────────────────────────────────
option(AICLI_USE_SYSTEM_CURL "Use system-installed libcurl" OFF)

# ── Git version ────────────────────────────────────────────────────────────
execute_process(
  COMMAND git describe --tags --always --dirty
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT GIT_VERSION)
  set(GIT_VERSION "unknown")
endif()
message(STATUS "Git version: ${GIT_VERSION}")

# ── Dependencies ───────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)

# ── Generate embedded JSON schema headers ──────────────────────────────────
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/filesystem.json
     FILESYSTEM_TOOLS_JSON_DATA)
configure_file(src/tools/filesystem_tools_json.h.in filesystem_tools_json.h)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/bash.json
     BASH_TOOLS_JSON_DATA)
configure_file(src/tools/bash_tools_json.h.in bash_tools_json.h)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/git.json
     GIT_TOOLS_JSON_DATA)
configure_file(src/tools/git_tools_json.h.in git_tools_json.h)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/default.json
     DEFAULT_TOOLS_JSON_DATA)
configure_file(src/tools/default_tools_json.h.in default_tools_json.h)

# ── Source files ───────────────────────────────────────────────────────────
add_executable(
  ai
  src/main.cpp
  src/chat.cpp
  src/system_prompt.cpp
  src/args.cpp
  src/config.cpp
  src/logging.cpp
  src/models.cpp
  src/openai.cpp
  src/response.cpp
  src/tools/filesystem.cpp
  src/tools/bash.cpp
  src/tools/git.cpp
  src/tools/default.cpp
  src/tool_calls.cpp
  src/utils.cpp
  src/base64.cpp)

# Platform-specific clipboard implementation
if(APPLE)
  target_sources(ai PRIVATE src/clip_macos.mm)
elseif(WIN32)
  target_sources(ai PRIVATE src/clip_win.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(ai PRIVATE src/clip_linux.cpp)
else()
  target_sources(ai PRIVATE src/clip_other.cpp)
endif()

set_target_properties(ai PROPERTIES OUTPUT_NAME "ai")

# ── Include directories ────────────────────────────────────────────────────
target_include_directories(
  ai
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
          ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
          ${CMAKE_CURRENT_BINARY_DIR})

target_compile_definitions(ai PRIVATE GIT_VERSION="${GIT_VERSION}")

# ── Platform-specific settings ─────────────────────────────────────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_definitions(ai PRIVATE UNICODE _UNICODE)
endif()

if(APPLE)
  target_link_libraries(ai PRIVATE "-framework AppKit" "-framework Foundation")
endif()

if(WIN32)
  target_link_libraries(ai PRIVATE "-luser32")
endif()

if(MINGW)
  target_link_options(ai PRIVATE -static-libgcc)
  set_target_properties(ai PROPERTIES LINK_FLAGS "-mconsole -municode")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(ai PRIVATE -static-libgcc -static-libstdc++)
endif()

# ── Link dependencies ──────────────────────────────────────────────────────
target_link_libraries(
  ai PRIVATE argparse::argparse base64::base64 subprocess::subprocess
             environment::environment utfx::utfx CURL::libcurl)

# ── Tests ──────────────────────────────────────────────────────────────────
if(AICLI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Install ────────────────────────────────────────────────────────────────
install(TARGETS ai RUNTIME DESTINATION bin)
