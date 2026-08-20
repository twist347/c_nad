set(NAD_TEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

function(nad_add_test target_name source_file)
    add_executable(${target_name} ${source_file})

    target_link_libraries(${target_name} PRIVATE nadc::nadc unity::framework)

    # test/support is header-only; this makes #include "support/..." work
    target_include_directories(${target_name} PRIVATE ${NAD_TEST_ROOT})
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -pedantic)

    add_test(NAME ${target_name} COMMAND ${target_name})

    # nad_calloc contracts on getting nullptr back for an oversized request, but
    # ASan aborts on one by default. Ignored when not built with a sanitizer.
    set_tests_properties(${target_name} PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=allocator_may_return_null=1")
endfunction()

add_subdirectory(algo)
add_subdirectory(alloc)
add_subdirectory(core)
add_subdirectory(ds)
