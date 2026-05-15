add_library(seqc STATIC
    slice/slice.c
    iter/iter.c
    vec/vec.c
    hashmap/hashmap.c
    string/string.c
    stack/stack.c
    queue/queue.c
    list/list.c
    set/set.c
    dlist/dlist.c
    bstree/bstree.c
    avl/avl.c
    omap/omap.c
    pqueue/pqueue.c
    ringbuf/ringbuf.c
)

target_include_directories(seqc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(seqc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/arena_allocator/include>
)

target_link_libraries(seqc PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/arena_allocator/lib/libarena.a
)

target_compile_options(seqc PRIVATE
    $<IF:$<C_COMPILER_ID:MSVC>,
        /W4,
        -Wall -Wextra -Wpedantic -Wno-unused-parameter>
)

install(TARGETS seqc ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/seqc DESTINATION include)
