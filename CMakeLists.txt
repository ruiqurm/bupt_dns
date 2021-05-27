cmake_minimum_required (VERSION 2.8...3.20.0)
project(bupt_dns)
set(CMAKE_C_STANDARD 11)
include_directories(
        ${PROJECT_SOURCE_DIR}/include/
)
# SET(CMAKE_C_COMPILER /usr/bin/clang)
link_libraries(ws2_32 wsock32)
add_subdirectory("src")
enable_testing()
add_subdirectory("test")
