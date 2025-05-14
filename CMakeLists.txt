cmake_minimum_required(VERSION 3.16)
project(spire VERSION 0.1 LANGUAGES C)

option(SPIRE_BUILD_EXAMPLES "Build examples" false)

add_library(${PROJECT_NAME} STATIC src/spire.c)

target_include_directories(spire
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(spire PUBLIC c_std_99)
target_compile_options(spire
    PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
        $<$<C_COMPILER_ID:MSVC>:/W4>
)

find_library(MATH_LIB m)
if (MATH_LIB)
    target_link_libraries(spire PUBLIC ${MATH_LIB})
endif ()

include(GNUInstallDirs)

install(TARGETS spire
    EXPORT spireTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT spireTargets
    FILE spireTargets.cmake
    NAMESPACE spire::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/spire
)

if (SPIRE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
