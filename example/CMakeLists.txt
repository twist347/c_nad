function(nad_add_example target_name source_file)
    add_executable(${target_name} ${source_file})

    target_link_libraries(${target_name} PRIVATE nadc::nadc)
    target_compile_options(${target_name} PRIVATE ${NAD_WARNINGS})

    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

add_subdirectory(algo)
add_subdirectory(alloc)
add_subdirectory(core)
add_subdirectory(ds)
