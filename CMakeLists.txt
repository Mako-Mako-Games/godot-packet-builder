cmake_minimum_required(VERSION 3.22)
project(packet_builder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(TARGET_DIRECTORY "${CMAKE_SOURCE_DIR}/addons/packet-builder/bin")

add_subdirectory(godot-cpp)

add_library(packet_builder SHARED
    src/register_types.cpp
    src/packet_schema.cpp
    src/bit_writer.cpp
    src/bit_reader.cpp
)

target_include_directories(packet_builder PRIVATE src/)
target_link_libraries(packet_builder godot-cpp)

# Platform/arch suffix matching Godot's naming convention
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(DEBUG_OR_RELEASE "release")
else()
    set(DEBUG_OR_RELEASE "debug")
endif()

if(UNIX AND NOT APPLE)
    set(PLATFORM "linux")
    set(ARCH "x86_64")
    set(SUFFIX ".so")
elseif(WIN32)
    set(PLATFORM "windows")
    set(ARCH "x86_64")
    set(SUFFIX ".dll")
elseif(APPLE)
    set(PLATFORM "macos")
    set(ARCH "universal")
    set(SUFFIX ".dylib")
endif()

set_target_properties(packet_builder PROPERTIES
    OUTPUT_NAME "libpacket_builder.${PLATFORM}.${DEBUG_OR_RELEASE}.${ARCH}"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${TARGET_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY "${TARGET_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${TARGET_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TARGET_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${TARGET_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${TARGET_DIRECTORY}"
)