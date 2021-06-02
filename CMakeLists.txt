cmake_minimum_required (VERSION 2.8...3.20.0)
project(bupt_dns)
set(CMAKE_C_STANDARD 11)
SET(CMAKE_C_FLAGS,"-target x86_64-pc-windows-gnu")

include_directories(
        ${PROJECT_SOURCE_DIR}/include/
)

# find_path(PTHREADW32_INCLUDE_DIR pthread.h)
# message(${PTHREADW32_INCLUDE_DIR})
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    link_libraries(ws2_32 wsock32)
endif()


add_subdirectory("src")
enable_testing()
add_subdirectory("test")
