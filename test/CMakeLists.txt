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
ADD_TEST(NAME test_cache COMMAND test_cache)
ADD_EXECUTABLE(test_cache test_cache.c)
TARGET_LINK_LIBRARIES(test_cache cachelib)

ADD_TEST(NAME test_hash COMMAND test_hash)
ADD_EXECUTABLE(test_hash test_hash.c)
TARGET_LINK_LIBRARIES(test_hash hash)
