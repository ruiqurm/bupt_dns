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

# FILE(COPY dnsrelay.txt DESTINATION ${PROJECT_BINARY_DIR}/Testing)
ADD_TEST(NAME test_loadrecord COMMAND test_loadrecord)
ADD_EXECUTABLE(test_loadrecord test_loadrecord.c)
TARGET_LINK_LIBRARIES(test_loadrecord localrecord)

ADD_TEST(NAME test_dns COMMAND test_dns)
ADD_EXECUTABLE(test_dns test_dns.c)
TARGET_LINK_LIBRARIES(test_dns adns)


ADD_EXECUTABLE(test_timeout test_timeout.c)
TARGET_LINK_LIBRARIES(test_timeout adns)