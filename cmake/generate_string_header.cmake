# cmake/generate_string_header.cmake

# ARGV0: Input file path ARGV1: Output header file path ARGV2: C++ variable name
# ARGV3: C++ namespace (optional)

if(NOT CMAKE_ARGC GREATER 5)
  message(
    FATAL_ERROR
      "Usage: cmake -P generate_string_header.cmake <input_file> <output_header> <variable_name> [namespace]"
  )
endif()

set(INPUT_FILE "${CMAKE_ARGV3}")
set(OUTPUT_HEADER "${CMAKE_ARGV4}")
set(VARIABLE_NAME "${CMAKE_ARGV5}")
set(CPP_NAMESPACE "")
if(ARGC GREATER 6)
  set(CPP_NAMESPACE "${CMAKE_ARGV6}")
endif()

if(NOT EXISTS "${INPUT_FILE}")
  message(FATAL_ERROR "Input file not found: ${INPUT_FILE}")
endif()

# 读取文件内容
file(READ "${INPUT_FILE}" FILE_CONTENT)

# 准备头文件内容
set(HEADER_CONTENT "#pragma once\n\n")
string(APPEND HEADER_CONTENT
       "#include <string_view> // C++17 for efficiency, or <string>\n\n")

if(NOT "${CPP_NAMESPACE}" STREQUAL "")
  string(APPEND HEADER_CONTENT "namespace ${CPP_NAMESPACE} {\n\n")
endif()

# 使用C++原始字符串字面量 (raw string literal) 这样可以保留换行和特殊字符，而无需转义
string(APPEND HEADER_CONTENT
       "constexpr std::string_view ${VARIABLE_NAME} = R\"(\n")
string(APPEND HEADER_CONTENT "${FILE_CONTENT}\n") # 加个换行以防文件末尾无换行
string(APPEND HEADER_CONTENT ")\";\n")

if(NOT "${CPP_NAMESPACE}" STREQUAL "")
  string(APPEND HEADER_CONTENT "\n} // namespace ${CPP_NAMESPACE}\n")
endif()

# 写入头文件
file(WRITE "${OUTPUT_HEADER}" "${HEADER_CONTENT}")
message(STATUS "Generated ${OUTPUT_HEADER} for variable ${VARIABLE_NAME}")
