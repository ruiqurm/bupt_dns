cmake_minimum_required(VERSION 2.8)
project(bupt_dns)
############################################################
# Create a library
############################################################

#Generate the shared library from the library sources
# add_library(hello_library SHARED 
#     src/Hello.cpp
# )
# add_library(hello::library ALIAS hello_library)

include_directories(
        ${PROJECT_SOURCE_DIR}/include/
)

AUX_SOURCE_DIRECTORY(./src DIR_SRCS)
############################################################
# Create an executable
############################################################
set(CMAKE_C_FLAGS,"-werror")

ADD_EXECUTABLE(main ${DIR_SRCS} )

