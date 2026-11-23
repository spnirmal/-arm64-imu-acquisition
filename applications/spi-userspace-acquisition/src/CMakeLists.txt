cmake_minimum_required(VERSION 3.16)

project(spi_userspace_acquisition VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(spi-userspace-acquisition
    main.cpp
    spi-device.cpp
    mpu9250.cpp
    gpio-layer.cpp
)

target_compile_definitions(spi-userspace-acquisition PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="spi-userspace-acquisition"
)

target_include_directories(spi-userspace-acquisition PRIVATE
    inc
)

target_link_libraries(spi-userspace-acquisition PRIVATE
    gpiod
)

install(TARGETS spi-userspace-acquisition DESTINATION bin)
