set(TRS_TEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

function(trs_add_test target_name source_file)
    add_executable(${target_name} ${source_file})

    target_link_libraries(${target_name} PRIVATE trs::dsa unity::framework)

    # test/support is header-only; this makes #include "support/..." work
    target_include_directories(${target_name} PRIVATE ${TRS_TEST_ROOT})
    target_compile_options(${target_name} PRIVATE ${TRS_WARNINGS})

    add_test(NAME ${target_name} COMMAND ${target_name})

    # trs_calloc contracts on getting nullptr back for an oversized request, but
    # ASan aborts on one by default. Ignored when not built with a sanitizer.
    set_tests_properties(${target_name} PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=allocator_may_return_null=1")
endfunction()

trs_add_test(test_dsa test_dsa.c)

add_subdirectory(algo)
add_subdirectory(alloc)
add_subdirectory(core)
add_subdirectory(ds)
