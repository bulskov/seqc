add_library(seqc STATIC
    arena/arena.c
    slice/slice.c
    iter/iter.c
    vec/vec.c
    hashmap/hashmap.c
)

target_include_directories(seqc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(seqc PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)
