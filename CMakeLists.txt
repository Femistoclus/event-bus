cmake_minimum_required(VERSION 3.14)
project(event_bus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(event_bus INTERFACE)
target_include_directories(event_bus INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(event_bus INTERFACE cxx_std_20)

add_executable(event_bus_demo src/main.cpp)
target_link_libraries(event_bus_demo PRIVATE event_bus)

find_package(Threads REQUIRED)
target_link_libraries(event_bus_demo PRIVATE Threads::Threads)