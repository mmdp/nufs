cmake_minimum_required(VERSION 2.6)
project(nufs)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_executable(nufs src/main.cpp src/mkfs.cpp src/common.cpp src/common.h src/mkfs.h src/fsprogs.cpp src/fsprogs.h src/structs.h)

install(TARGETS nufs RUNTIME DESTINATION bin)
