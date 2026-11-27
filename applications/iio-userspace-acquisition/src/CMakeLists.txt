cmake_minimum_required(VERSION 3.16)

project(iio_userspace_acquisition VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(iio-userspace-acquisition
    main.cpp
    iio-device.cpp
)

target_compile_definitions(iio-userspace-acquisition PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="iio-userspace-acquisition"
)

target_include_directories(iio-userspace-acquisition PRIVATE
    inc
)

install(TARGETS iio-userspace-acquisition DESTINATION bin)
