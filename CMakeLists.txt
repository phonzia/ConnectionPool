cmake_minimum_required(VERSION 3.0.0)

## project
project(ConnectionPool LANGUAGES CXX)

## C++ standard
set(CMAKE_CXX_STANDARD 14)

include(ExternalProject)

set(CONNECTIONPOOL_TARGET_NAME        ${PROJECT_NAME})
set(CONNECTIONPOOL_TARGET_SOURCE_DIR  "src")

## Target
add_library(${CONNECTIONPOOL_TARGET_NAME} INTERFACE)
target_include_directories(
        ${CONNECTIONPOOL_TARGET_NAME}
        INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

ExternalProject_Add(amalgamate
        GIT_REPOSITORY "https://github.com/theodelrieu/Amalgamate"
        CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}"
)

# There is no way to tell amalgamate to force-write the output file even if it already exists...
add_custom_target(single_header ALL rm -f "${CMAKE_SOURCE_DIR}/single_header/connection_pool.hpp"
        COMMENT "Amalgamating json.hpp..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/${CONNECTIONPOOL_TARGET_SOURCE_DIR}
        DEPENDS amalgamate
        COMMAND "${CMAKE_BINARY_DIR}/bin/amalgamate" -w '*.hpp' -i . connection_pool.hpp "${CMAKE_SOURCE_DIR}/single_header/connection_pool.hpp"
)

add_subdirectory(test)