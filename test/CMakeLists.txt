include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
# Don't install gtest alongside the project
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_gtest name source)
    add_executable(${name} ${source})
    set_source_files_properties(${source} PROPERTIES LANGUAGE CXX)
    target_compile_features(${name} PRIVATE cxx_std_17)
    target_compile_options(${name} PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra>)
    target_link_libraries(${name} PRIVATE seqc GTest::gtest_main)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_gtest(slice_test   slice/slice_test.c)
add_gtest(iter_test    iter/iter_test.c)
add_gtest(vec_test     vec/vec_test.c)
add_gtest(hashmap_test hashmap/hashmap_test.c)
add_gtest(string_test  string/string_test.c)
add_gtest(string_io_test string/string_io_test.c)
add_gtest(stack_test   stack/stack_test.c)
add_gtest(queue_test   queue/queue_test.c)
add_gtest(list_test    list/list_test.c)
add_gtest(set_test     set/set_test.c)
add_gtest(dlist_test   dlist/dlist_test.c)
add_gtest(bstree_test  bstree/bstree_test.c)
add_gtest(avl_test     avl/avl_test.c)
add_gtest(omap_test    omap/omap_test.c)
add_gtest(pqueue_test  pqueue/pqueue_test.c)
add_gtest(ringbuf_test ringbuf/ringbuf_test.c)
