project(ctx)
cmake_minimum_required(VERSION 2.8)

option(CTX_VALGRIND "CTX_VALGRIND" OFF)
if (${CTX_VALGRIND})
  set(CTX_ENABLE_VALGRIND 1)
else()
  set(CTX_ENABLE_VALGRIND 0)
endif()

option(CTX_ASAN "CTX_ASAN" OFF)
if (${CTX_VALGRIND})
  set(CTX_ENABLE_ASAN 1)
else()
  set(CTX_ENABLE_ASAN 0)
endif()

configure_file (
  "include/ctx/ctx_config.h.in"
  "${CMAKE_BINARY_DIR}/generated/ctx_config.h"
)

find_package(Boost COMPONENTS system thread context REQUIRED)
find_package(Threads)

include_directories(SYSTEM ${Boost_INCLUDE_DIR})
include_directories(include)
include_directories(${CMAKE_BINARY_DIR}/generated)

add_library(ctx src/ctx.cc)
target_link_libraries(ctx ${Boost_CONTEXT_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY})
if (NOT MSVC)
  set_target_properties(ctx PROPERTIES COMPILE_FLAGS "-std=c++14 -Wall -Wextra")
endif()

set(example-targets "")
file(GLOB_RECURSE example_files example/*.cc)
foreach(example_file ${example_files})
  get_filename_component(example ${example_file} NAME_WE)
  add_executable(${example} EXCLUDE_FROM_ALL ${example_file})
  target_link_libraries(${example} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ctx)
  set_target_properties(${example} PROPERTIES COMPILE_FLAGS "-std=c++1y -Wall -Wextra")

  list(APPEND example-targets ${example})
endforeach()

add_custom_target(examples)
add_dependencies(examples ${example-targets})
