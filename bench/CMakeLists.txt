function(trs_add_bench target_name source_file)
    add_executable(${target_name} ${source_file})

    target_link_libraries(${target_name} PRIVATE trs::ds)
    target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/ubench)
    target_compile_options(${target_name} PRIVATE ${TRS_WARNINGS})

    set_target_properties(${target_name} PROPERTIES C_EXTENSIONS ON)
endfunction()

trs_add_bench(bench_ds_vec ds/bench_vec.c)
