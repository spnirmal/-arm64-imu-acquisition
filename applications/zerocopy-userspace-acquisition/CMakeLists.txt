cmake_minimum_required(VERSION 3.10)
project(zerocopy-userspace-acquisition CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(zerocopy-consumer zerocopy_consumer.cpp)

install(TARGETS zerocopy-consumer DESTINATION bin)
