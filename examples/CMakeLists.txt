cmake_minimum_required(VERSION 3.16)
project(spire_examples LANGUAGES C)

function(add_example name sources)
    add_executable(${name} ${sources})
    target_compile_features(${name} PRIVATE c_std_99)
    target_compile_options(${name}
        PRIVATE
            $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
            $<$<C_COMPILER_ID:MSVC>:/W4>
    )
    target_link_libraries(${name} PRIVATE spire)
endfunction()

add_example(hash_map hash_map.c)
