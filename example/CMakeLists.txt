# The examples are what the reference quotes through @snippet. Building them is what keeps
# the documentation from drifting; running them is what keeps it honest.
function(nad_add_example target_name source_file)
    add_executable(${target_name} ${source_file})

    target_link_libraries(${target_name} PRIVATE nadc::nadc)
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -pedantic)

    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

add_subdirectory(alloc)
add_subdirectory(core)
add_subdirectory(ds)
