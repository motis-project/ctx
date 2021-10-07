project(ctx)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(cmake/pkg.cmake)

find_package(Threads)

option(CTX_VALGRIND "CTX_VALGRIND" OFF)
if (${CTX_VALGRIND})
  set(CTX_ENABLE_VALGRIND 1)
else()
  set(CTX_ENABLE_VALGRIND 0)
endif()

option(CTX_ASAN "CTX_ASAN" OFF)
if (${CTX_ASAN})
  message(STATUS "Address Sanitizer support enabled")
  set(CTX_ENABLE_ASAN 1)
else()
  set(CTX_ENABLE_ASAN 0)
endif()

configure_file (
  "include/ctx/ctx_config.h.in"
  "${CMAKE_BINARY_DIR}/generated/ctx_config.h"
)

add_library(ctx src/ctx.cc)
target_link_libraries(ctx fcontext boost utl)
target_include_directories(ctx PUBLIC include ${CMAKE_BINARY_DIR}/generated)
target_compile_features(ctx PUBLIC cxx_std_17)
if (NOT MSVC)
  set_target_properties(ctx PROPERTIES COMPILE_FLAGS "-Wall -Wextra")
endif()
if (MSVC)
  target_compile_definitions(ctx PUBLIC _WIN32_WINNT=0x0601)
endif()

set(example-targets "")
file(GLOB_RECURSE example_files example/*.cc)
foreach(example_file ${example_files})
  get_filename_component(example ${example_file} NAME_WE)
  add_executable(${example} EXCLUDE_FROM_ALL ${example_file})
  target_link_libraries(${example} ${CMAKE_THREAD_LIBS_INIT} ctx)
  target_compile_features(${example} PUBLIC cxx_std_17)
  if (NOT MSVC)
    set_target_properties(${example} PROPERTIES COMPILE_FLAGS "-Wall -Wextra")
  endif()

  list(APPEND example-targets ${example})
endforeach()

add_custom_target(examples)
add_dependencies(examples ${example-targets})
