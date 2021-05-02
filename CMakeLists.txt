cmake_minimum_required(VERSION 2.8)
SET(CMAKE_C_COMPILER /usr/bin/clang)
project(bupt_dns)
include_directories(
        ${PROJECT_SOURCE_DIR}/include/
)
add_subdirectory(src)

enable_testing()
add_subdirectory(test)
add_test(NAME Mytest COMMAND Test)


