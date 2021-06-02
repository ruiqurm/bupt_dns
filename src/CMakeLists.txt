include_directories(
        ${PROJECT_SOURCE_DIR}/include/
)
set(CMAKE_C_FLAGS,"-werror")
aux_source_directory(. DIR_SRCS)

# IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
#     target_link_libraries(cachelib m)
#     target_link_libraries(hash m)
#     target_link_libraries(localrecord m)
#     target_link_libraries(adns m)
# endif()
link_libraries(m)
add_library(cachelib STATIC cache.c hash.c log.c)
add_library(hash STATIC hash.c)
add_library(localrecord STATIC local_record.c cache.c hash.c log.c)
add_library(adns STATIC local_record.c common.c cache.c hash.c log.c)



ADD_EXECUTABLE(dns ${DIR_SRCS})
# target_link_libraries(dns pthread)
