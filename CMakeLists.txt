cmake_minimum_required(VERSION 3.16)
project(spire LANGUAGES C)

add_library(${PROJECT_NAME} STATIC src/spire.c)

target_include_directories(spire
    PUBLIC  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>"
    PRIVATE "src/"
)

set_target_properties(spire PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
)

if (MSVC)
    add_compile_options("/W4")
else ()
    add_compile_options("-Wall -Wextra -Wpedantic")
endif ()
