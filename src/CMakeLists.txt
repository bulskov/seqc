add_library(seqc STATIC
    arena/arena.c
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

target_include_directories(seqc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(seqc PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)
