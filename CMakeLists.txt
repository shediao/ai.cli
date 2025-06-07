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

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    if(APPLE OR LINUX)
      set(CMAKE_CXX_FLAGS_DEBUG
          "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls"
      )
    endif()

  endif()
endif()
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /utf-8 /EHsc")
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(AICLI_BUILD_TESTS "Set to ON to build tests" ON)
else()
  option(AICLI_BUILD_TESTS "Set to OFF to build tests" OFF)
endif()

# 添加FetchContent支持
include(FetchContent)

# 添加argparse用于命令行参数解析
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/shediao/argparse.hpp
  GIT_TAG v0.0.9)
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
  GIT_TAG v0.0.3)
FetchContent_MakeAvailable(subprocess)

FetchContent_Declare(
  environment
  GIT_REPOSITORY https://github.com/shediao/environment.hpp
  GIT_TAG v0.0.1)
FetchContent_MakeAvailable(environment)

# 添加libcurl用于HTTP请求
find_package(CURL REQUIRED)

set(GENERATOR_SCRIPT
    ${CMAKE_CURRENT_LIST_DIR}/cmake/generate_string_header.cmake)
# ARGS: INPUT_FILE_PATH: 绝对路径或相对于CMAKE_CURRENT_SOURCE_DIR的路径
# OUTPUT_HEADER_REL_PATH: 生成的头文件相对于CMAKE_CURRENT_BINARY_DIR的路径 CPP_VAR_NAME:
# 在C++代码中使用的变量名 CPP_NAMESPACE (optional): C++ 命名空间
function(embed_text_file_as_string INPUT_FILE_PATH OUTPUT_HEADER_REL_PATH
         CPP_VAR_NAME)
  get_filename_component(ABS_INPUT_FILE ${INPUT_FILE_PATH} ABSOLUTE)
  set(ABS_OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_HEADER_REL_PATH}")

  # 确保输出目录存在
  get_filename_component(OUTPUT_DIR ${ABS_OUTPUT_HEADER} DIRECTORY)
  file(MAKE_DIRECTORY ${OUTPUT_DIR})

  set(NAMESPACE_ARG "")
  if(ARGC GREATER 3) # 检查是否有第四个参数 (namespace)
    set(NAMESPACE_ARG ${ARGV[3]})
  endif()

  add_custom_command(
    OUTPUT ${ABS_OUTPUT_HEADER}
    COMMAND
      ${CMAKE_COMMAND} -P ${GENERATOR_SCRIPT} "${ABS_INPUT_FILE}"
      "${ABS_OUTPUT_HEADER}" "${CPP_VAR_NAME}" "${NAMESPACE_ARG}" # 可以是空字符串
    DEPENDS ${ABS_INPUT_FILE} ${GENERATOR_SCRIPT}
    COMMENT
      "Generating ${OUTPUT_HEADER_REL_PATH} for ${CPP_VAR_NAME} from ${INPUT_FILE_PATH}"
    VERBATIM # 确保命令中的特殊字符被正确处理
  )

  # 创建一个自定义目标，以便其他目标可以依赖它 使用唯一的名称，例如基于变量名
  string(MAKE_C_IDENTIFIER "GenTarget_${CPP_VAR_NAME}" SAFE_TARGET_NAME)
  add_custom_target(${SAFE_TARGET_NAME} ALL DEPENDS ${ABS_OUTPUT_HEADER})

  # 将生成头文件的目录添加到函数的调用者作用域，以便用于 include_directories
  set(${CPP_VAR_NAME}_GENERATED_INCLUDE_DIR
      ${OUTPUT_DIR}
      PARENT_SCOPE)
  set(${CPP_VAR_NAME}_GENERATED_TARGET_NAME
      ${SAFE_TARGET_NAME}
      PARENT_SCOPE)
endfunction()

embed_text_file_as_string(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/filesystem.json"
  "generated_headers/filesystem_tools_json.h" "filesystem_tools_json_str")

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

# 添加头文件路径
target_include_directories(
  ai
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
          ${filesystem_tools_json_str_GENERATED_INCLUDE_DIR})

# 链接依赖库
target_link_libraries(
  ai PRIVATE argparse::argparse base64::base64 subprocess::subprocess
             environment::environment CURL::libcurl)

if(APPLE)
  target_link_libraries(ai PRIVATE "-framework AppKit")
elseif(WIN32)
  target_link_libraries(ai PRIVATE "-luser32")
endif()

add_dependencies(ai ${filesystem_tools_json_str_GENERATED_TARGET_NAME})

if(AICLI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# 安装目标
install(TARGETS ai RUNTIME DESTINATION bin)
