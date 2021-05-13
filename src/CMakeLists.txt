
include_directories(
        ${PROJECT_SOURCE_DIR}/include/
)
set(CMAKE_C_FLAGS,"-werror")
aux_source_directory(. DIR_SRCS)
# ADD_EXECUTABLE(dns main.c)
add_library(cachelib STATIC cache.c)

ADD_EXECUTABLE(dns ${DIR_SRCS})
