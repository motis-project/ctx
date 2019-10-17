set(CMAKE_USER_MAKE_RULES_OVERRIDE
   ${CMAKE_CURRENT_SOURCE_DIR}/cmake/c_flag_overrides.cmake)
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
   ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cxx_flag_overrides.cmake)

project(ctx)
cmake_minimum_required(VERSION 3.13)

add_subdirectory(external_lib/deboost.context)

find_package(Threads)
if (MSVC)
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_USE_MULTITHREADED ON)
  set(Boost_USE_STATIC_RUNTIME ON)
endif()
find_package(Boost COMPONENTS system thread REQUIRED)
link_directories(${Boost_LIBRARY_DIRS})

option(CTX_VALGRIND "CTX_VALGRIND" OFF)
if (${CTX_VALGRIND})
  set(CTX_ENABLE_VALGRIND 1)
else()
  set(CTX_ENABLE_VALGRIND 0)
endif()

option(CTX_ASAN "CTX_ASAN" OFF)
if (${CTX_ASAN})
  set(CTX_ENABLE_ASAN 1)
else()
  set(CTX_ENABLE_ASAN 0)
endif()

configure_file (
  "include/ctx/ctx_config.h.in"
  "${CMAKE_BINARY_DIR}/generated/ctx_config.h"
)

add_library(ctx src/ctx.cc)
target_link_libraries(ctx fcontext ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY})
target_include_directories(ctx
  PUBLIC include ${CMAKE_BINARY_DIR}/generated ${Boost_INCLUDE_DIR})
target_compile_features(ctx PUBLIC cxx_std_17)
if (NOT MSVC)
  set_target_properties(ctx PROPERTIES COMPILE_FLAGS "-Wall -Wextra")
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
