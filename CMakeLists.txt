# Using the same minimum as the godot-cpp project
cmake_minimum_required(VERSION 3.17)
# Silence unused variable warning when specified from toolchain
if(CMAKE_C_COMPILER)
endif()
set(LIBNAME "packet_builder")
set(GODOT_PROJECT_DIR "." CACHE STRING "The directory of a Godot project folder")
# Make sure all the dependencies are satisfied
find_package(Python3 3.4 REQUIRED)
find_program(GIT git REQUIRED)
# Ensure godot-cpp submodule has been updated
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/src")
    message(NOTICE "godot-cpp bindings source not found")
    message(NOTICE "initializing/updating the godot-cpp submodule...")
    execute_process(
            COMMAND git submodule update --init godot-cpp
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND_ERROR_IS_FATAL ANY
    )
endif()
add_subdirectory(godot-cpp SYSTEM)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp/gen/include")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${godot-cpp_SOURCE_DIR}/cmake")
include(GodotCPPModule)
get_target_property(GODOTCPP_SUFFIX godot::cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_PLATFORM godot::cpp GODOTCPP_PLATFORM)
project(packet_builder
        VERSION 1.0
        DESCRIPTION "Packet builder GDExtension for Godot 4."
        LANGUAGES CXX
)
add_library(${LIBNAME} SHARED)
target_sources(${LIBNAME}
        PRIVATE
        src/register_types.cpp
        src/field_encoder.cpp
        src/packet_builder.cpp
        src/encoders/int_encoder.cpp
        src/encoders/bool_encoder.cpp
        src/encoders/float_encoder.cpp
        src/encoders/vector3_encoder.cpp
        src/encoders/transform_encoder.cpp
        src/encoders/string_encoder.cpp
        src/bit_writer.cpp
        src/bit_reader.cpp
        src/bit_reader.hpp
)
target_include_directories(${LIBNAME} PRIVATE src/ src/encoders/)
# Fetch a list of the xml files to use for documentation and add to our target
file(GLOB_RECURSE DOC_XML LIST_DIRECTORIES NO CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/doc_classes/*.xml")
# Conditionally add doc data to compile output
if(DOC_XML)
    if(GODOTCPP_TARGET MATCHES "editor|template_debug")
        target_doc_sources(${LIBNAME} "${DOC_XML}")
    endif()
endif()
target_link_libraries(${LIBNAME} PRIVATE godot-cpp)
set_property(TARGET ${LIBNAME} PROPERTY CXX_STANDARD 17)
set(TARGET_DIRECTORY "${PROJECT_SOURCE_DIR}/addons/packet-builder/bin/${GODOTCPP_PLATFORM}")
set_target_properties(${LIBNAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$<1:${TARGET_DIRECTORY}>"
        PREFIX ""
        OUTPUT_NAME "${LIBNAME}${GODOTCPP_SUFFIX}"
)
set(GODOT_PROJECT_BINARY_DIR "${PROJECT_SOURCE_DIR}/${GODOT_PROJECT_DIR}/addons/packet-builder/bin/${GODOTCPP_PLATFORM}")
add_custom_command(TARGET ${LIBNAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${LIBNAME}>" "${GODOT_PROJECT_BINARY_DIR}/$<TARGET_FILE_NAME:${LIBNAME}>"
)