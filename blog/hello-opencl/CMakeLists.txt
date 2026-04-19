cmake_minimum_required(VERSION 3.16)
project(opencl_vector_add LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCL QUIET)

add_executable(opencl_vector_add opencl_vector_add.cpp)
target_compile_definitions(
  opencl_vector_add
  PRIVATE OPENCL_TUTORIAL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

if(OpenCL_FOUND)
  target_link_libraries(opencl_vector_add PRIVATE OpenCL::OpenCL)
else()
  if(NOT DEFINED OPENCL_INCLUDE_DIR AND DEFINED ENV{OPENCL_INCLUDE_DIR})
    set(OPENCL_INCLUDE_DIR "$ENV{OPENCL_INCLUDE_DIR}")
  endif()

  if(NOT DEFINED OPENCL_LIBRARY AND DEFINED ENV{OPENCL_LIBRARY})
    set(OPENCL_LIBRARY "$ENV{OPENCL_LIBRARY}")
  endif()

  if(NOT OPENCL_INCLUDE_DIR OR NOT OPENCL_LIBRARY)
    message(
      FATAL_ERROR
        "OpenCL not found. Set OPENCL_INCLUDE_DIR and OPENCL_LIBRARY explicitly.")
  endif()

  target_include_directories(opencl_vector_add PRIVATE "${OPENCL_INCLUDE_DIR}")
  target_link_libraries(opencl_vector_add PRIVATE "${OPENCL_LIBRARY}")
endif()

if(ANDROID)
  target_link_libraries(opencl_vector_add PRIVATE dl log)
endif()

add_custom_command(
  TARGET opencl_vector_add
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/vector_add.cl"
    "$<TARGET_FILE_DIR:opencl_vector_add>/vector_add.cl")
