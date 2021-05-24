include_directories(
        ${PROJECT_SOURCE_DIR}/include/
)
# add_library(cachelib
#               STATIC
#               ../src/cache.c)
set(CMAKE_C_FLAGS,-werror
                  -O2
                  )
# aux_source_directory(. DIR_SRCS)
ADD_EXECUTABLE(Test test.c)
TARGET_LINK_LIBRARIES(Test cachelib)
