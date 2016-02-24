project(ctx)
cmake_minimum_required(VERSION 2.8)

file(GLOB_RECURSE ctx-files src/*)
add_library(ctx ${ctx-files})

add_executable(ctx-example example/main.cc)
target_link_libraries(ctx-example ctx)
