cmake_minimum_required(VERSION 3.16)
project(spire_tests LANGUAGES C)

add_executable(spire_tests main.c hash_map.c)
target_compile_features(spire_tests PRIVATE c_std_99)
target_compile_options(spire_tests
    PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
target_link_libraries(spire_tests PRIVATE spire)
