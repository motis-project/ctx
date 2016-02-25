project(ctx)
cmake_minimum_required(VERSION 2.8)

find_package(Boost COMPONENTS system thread context REQUIRED)
find_package(Threads)

include_directories(include)

file(GLOB_RECURSE ctx-files src/*)
add_library(ctx ${ctx-files})
target_link_libraries(ctx ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(ctx PROPERTIES COMPILE_FLAGS "-std=c++1y -Wall -Wextra")

add_executable(ctx-example example/main.cc)
target_link_libraries(ctx-example ctx)
set_target_properties(ctx-example PROPERTIES COMPILE_FLAGS "-std=c++1y -Wall -Wextra")
