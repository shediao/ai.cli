cmake_minimum_required(VERSION 3.20)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  cmake_policy(SET CMP0135 NEW)
endif()

project(
  ai_cli
  VERSION 0.1.0
  LANGUAGES CXX)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Debug
      CACHE
        STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel..."
        FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT MSYS)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /utf-8 /EHsc")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang" AND NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
endif()

if(NOT CMAKE_CROSSCOMPILING AND CMAKE_HOST_SYSTEM_NAME MATCHES "Linux|Darwin")
  option(AICLI_ENABLE_ASASN "Set to ON to build tests" ON)
else()
  option(AICLI_ENABLE_ASASN "Set to ON to build tests" OFF)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(AICLI_BUILD_TESTS "Set to ON to build tests" ON)
else()
  option(AICLI_BUILD_TESTS "Set to OFF to build tests" OFF)
endif()

if(AICLI_ENABLE_ASASN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls"
    )
  endif()
endif()

# 添加FetchContent支持
include(FetchContent)

# 添加argparse用于命令行参数解析
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/shediao/argparse.hpp
  GIT_TAG v0.0.10)
FetchContent_MakeAvailable(argparse)

# 添加base64用于嵌入文件
FetchContent_Declare(
  base64
  GIT_REPOSITORY https://github.com/shediao/base64.hpp
  GIT_TAG v0.0.1)
FetchContent_MakeAvailable(base64)

FetchContent_Declare(
  subprocess
  GIT_REPOSITORY https://github.com/shediao/subprocess.hpp
  GIT_TAG v0.0.5)
FetchContent_MakeAvailable(subprocess)

FetchContent_Declare(
  environment
  GIT_REPOSITORY https://github.com/shediao/environment.hpp
  GIT_TAG v0.0.4)
FetchContent_MakeAvailable(environment)

# 添加libcurl用于HTTP请求
find_package(CURL REQUIRED)

# 添加源文件
add_executable(
  ai
  src/main.cpp
  src/chat.cpp
  src/args.cpp
  src/logging.cpp
  src/models.cpp
  src/openai.cpp
  src/response.cpp
  src/tools/filesystem.cpp
  src/tool_calls.cpp
  src/utils.cpp
  src/base64.cpp)

if(APPLE)
  target_sources(ai PRIVATE src/clip_macos.mm)
elseif(WIN32)
  target_sources(ai PRIVATE src/clip_win.cpp)
elseif(LINUX)
  target_sources(ai PRIVATE src/clip_linux.cpp)
else()
  target_sources(ai PRIVATE src/clip_other.cpp)
endif()

set_target_properties(ai PROPERTIES OUTPUT_NAME "ai")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/filesystem.json
     FILESYSTEM_TOOLS_JSON_DATA)
configure_file(src/tools/filesystem_tools_json.h.in filesystem_tools_json.h)

# 添加头文件路径
target_include_directories(
  ai
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
          ${CMAKE_CURRENT_BINARY_DIR})

# 链接依赖库
target_link_libraries(
  ai PRIVATE argparse::argparse base64::base64 subprocess::subprocess
             environment::environment CURL::libcurl)

if(APPLE)
  target_link_libraries(ai PRIVATE "-framework AppKit")
elseif(WIN32)
  target_link_libraries(ai PRIVATE "-luser32")
endif()

if(AICLI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# 安装目标
install(TARGETS ai RUNTIME DESTINATION bin)
