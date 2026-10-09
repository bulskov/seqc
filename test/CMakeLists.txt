include(FetchContent)

set(SEQC_CTT_GIT_TAG "v0.3.1"
    CACHE STRING "Tag or commit of the ctt test library")

FetchContent_Declare(
    ctt
    GIT_REPOSITORY https://github.com/bulskov/ctt.git
    GIT_TAG        ${SEQC_CTT_GIT_TAG}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ctt)

function(add_ctt_test name source)
    add_executable(${name} ${source})
    target_compile_options(${name} PRIVATE
        $<IF:$<BOOL:${MSVC}>,/W4,-Wall -Wextra>)
    target_link_libraries(${name} PRIVATE seqc ctt::ctt)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_ctt_test(slice_test     slice/slice_test.c)
add_ctt_test(iter_test      iter/iter_test.c)
add_ctt_test(vec_test       vec/vec_test.c)
add_ctt_test(hashmap_test   hashmap/hashmap_test.c)
add_ctt_test(string_test    string/string_test.c)
add_ctt_test(string_io_test string/string_io_test.c)
add_ctt_test(string_search_test string/string_search_test.c)
add_ctt_test(stack_test     stack/stack_test.c)
add_ctt_test(queue_test     queue/queue_test.c)
add_ctt_test(list_test      list/list_test.c)
add_ctt_test(set_test       set/set_test.c)
add_ctt_test(dlist_test     dlist/dlist_test.c)
add_ctt_test(bstree_test    bstree/bstree_test.c)
add_ctt_test(avl_test       avl/avl_test.c)
add_ctt_test(omap_test      omap/omap_test.c)
add_ctt_test(pqueue_test    pqueue/pqueue_test.c)
add_ctt_test(ringbuf_test   ringbuf/ringbuf_test.c)
add_ctt_test(api_test       api/api_test.c)
