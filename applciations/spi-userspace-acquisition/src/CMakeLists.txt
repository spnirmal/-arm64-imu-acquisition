cmake_minimum_required(VERSION 3.16)

project(spi_userspace_acquisition LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(spi-userspace-acquisition
    main.cpp
    spi-device.cpp
    mpu9250.cpp
)

target_include_directories(spi-userspace-acquisition PRIVATE
    inc
)
