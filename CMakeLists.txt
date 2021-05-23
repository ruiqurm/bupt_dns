cmake_minimum_required(VERSION 2.8)
project(bupt_dns)
# SET(CMAKE_C_COMPILER /usr/bin/clang)

include_directories(
        ${PROJECT_SOURCE_DIR}/include/
)
link_libraries(ws2_32 wsock32)
add_subdirectory(src)

enable_testing()
add_subdirectory(test)
add_test(NAME Mytest COMMAND Test)


