cmake_minimum_required(VERSION 3.20)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  cmake_policy(SET CMP0135 NEW)
endif()

project(
  aichat_cli
  VERSION 0.1.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls"
)

# 添加FetchContent支持
include(FetchContent)

# 添加argparse用于命令行参数解析
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/shediao/argparse.hpp
  GIT_TAG main)
FetchContent_MakeAvailable(argparse)

# 添加nlohmann/json用于JSON处理
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# 添加libcurl用于HTTP请求
find_package(CURL REQUIRED)

# 添加源文件
add_executable(aichat src/main.cpp src/openai_client.cpp src/cli_parser.cpp)

if(APPLE)
  target_sources(aichat PRIVATE src/clip_macos.mm)
elseif(WIN32)
  target_sources(aichat PRIVATE src/clip_win.cpp)
endif()

set_target_properties(aichat PROPERTIES OUTPUT_NAME "aichat")

# 添加头文件路径
target_include_directories(aichat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(APPLE)
  target_include_directories(
    aichat PRIVATE "/opt/homebrew/Cellar/readline/8.2.13/include")

  target_link_directories(aichat PRIVATE
                          "/opt/homebrew/Cellar/readline/8.2.13/lib")

  target_compile_definitions(aichat PRIVATE _DARWIN_C_SOURCE)
endif()

# 链接依赖库
target_link_libraries(
  aichat PRIVATE argparse::argparse nlohmann_json::nlohmann_json CURL::libcurl
                 -lreadline)

if(APPLE)
  target_link_libraries(aichat PRIVATE "-framework AppKit")
elseif(WIN32)
  target_link_libraries(aichat PRIVATE "-luser32")
endif()

# 安装目标
install(TARGETS aichat RUNTIME DESTINATION bin)
