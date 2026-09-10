cmake_minimum_required(VERSION 3.22)

project(terse-dsa
        VERSION 0.1.0
        DESCRIPTION "Terse data structures and algorithms in C23"
        LANGUAGES C
)

if (PROJECT_IS_TOP_LEVEL)
    if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif ()
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_C_EXTENSIONS OFF)
endif ()

set(TRS_C_STANDARD 23)

option(TRS_BUILD_SHARED "Build trs_dsa as a shared library" ${PROJECT_IS_TOP_LEVEL})
option(TRS_BUILD_TEST "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(TRS_BUILD_EXAMPLE "Build the examples the reference quotes" ${PROJECT_IS_TOP_LEVEL})
option(TRS_BUILD_BENCH "Build the benchmarks" OFF)

set(TRS_WARNINGS
        $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
        $<$<C_COMPILER_ID:MSVC>:/W4> #unused for now
)

set(TRS_ALGO_SOURCES
        src/algo/compare.c
        src/algo/copy.c
        src/algo/fill.c
        src/algo/fold.c
        src/algo/heap.c
        src/algo/merge.c
        src/algo/modify.c
        src/algo/permute.c
        src/algo/search.c
        src/algo/set.c
        src/algo/sort.c
        src/algo/transform.c
)

set(TRS_ALLOC_SOURCES
        src/alloc/aligned.c
        src/alloc/alloc.c
        src/alloc/arena.c
        src/alloc/default.c
        src/alloc/log.c
        src/alloc/pool.c
)

set(TRS_CORE_SOURCES
        src/core/cmp.c
        src/core/hash.c
        src/core/print.c
        src/core/rng.c
        src/core/span.c
        src/core/status.c
)

set(TRS_DS_SOURCES
        src/ds/arr.c
        src/ds/bitset.c
        src/ds/deque.c
        src/ds/hmap.c
        src/ds/hset.c
        src/ds/list.c
        src/ds/pqueue.c
        src/ds/queue.c
        src/ds/stack.c
        src/ds/vec.c
)

set(TRS_SOURCES
        ${TRS_ALGO_SOURCES}
        ${TRS_ALLOC_SOURCES}
        ${TRS_CORE_SOURCES}
        ${TRS_DS_SOURCES}
)

if (TRS_BUILD_SHARED)
    set(TRS_LIBRARY_TYPE SHARED)
else ()
    set(TRS_LIBRARY_TYPE STATIC)
endif ()

add_library(trs_dsa ${TRS_LIBRARY_TYPE} ${TRS_SOURCES})
add_library(trs::dsa ALIAS trs_dsa)

target_include_directories(trs_dsa
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(trs_dsa PUBLIC c_std_${TRS_C_STANDARD})
target_compile_options(trs_dsa PRIVATE ${TRS_WARNINGS})

if (UNIX)
    target_link_libraries(trs_dsa PUBLIC m)
endif ()

target_compile_definitions(trs_dsa
        PRIVATE TRS_BUILD
        PUBLIC $<$<NOT:$<BOOL:${TRS_BUILD_SHARED}>>:TRS_STATIC>
)

set_target_properties(trs_dsa PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        POSITION_INDEPENDENT_CODE ON
        C_EXTENSIONS OFF
        C_VISIBILITY_PRESET hidden
)

include(CheckIPOSupported)
check_ipo_supported(RESULT TRS_IPO_SUPPORTED OUTPUT TRS_IPO_ERROR)
if (TRS_IPO_SUPPORTED)
    set_target_properties(trs_dsa PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON
    )
else ()
    message(STATUS "  LTO unavailable : ${TRS_IPO_ERROR}")
endif ()

if (TRS_BUILD_TEST OR TRS_BUILD_EXAMPLE)
    enable_testing()
endif ()

if (TRS_BUILD_TEST)
    set(TRS_UNITY_DIR ${PROJECT_SOURCE_DIR}/thirdparty/Unity-2.7.0)
    add_subdirectory(${TRS_UNITY_DIR} EXCLUDE_FROM_ALL)
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE UNITY_INCLUDE_FLOAT)

    add_subdirectory(test)
endif ()

if (TRS_BUILD_EXAMPLE)
    add_subdirectory(example)
endif ()

if (TRS_BUILD_BENCH)
    add_subdirectory(bench)
endif ()

if (PROJECT_IS_TOP_LEVEL)
    string(TOLOWER ${TRS_LIBRARY_TYPE} TRS_LIBRARY_TYPE_LOWER)
    message(STATUS "")
    message(STATUS "terse-dsa configuration:")
    message(STATUS "  version        : ${PROJECT_VERSION}")
    message(STATUS "  build type     : ${CMAKE_BUILD_TYPE}")
    message(STATUS "  library        : ${TRS_LIBRARY_TYPE_LOWER}")
    message(STATUS "  compiler       : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
    message(STATUS "  C standard     : C${TRS_C_STANDARD}")
    message(STATUS "  tests          : ${TRS_BUILD_TEST}")
    message(STATUS "  examples       : ${TRS_BUILD_EXAMPLE}")
    message(STATUS "  benchmarks     : ${TRS_BUILD_BENCH}")
    message(STATUS "")
endif ()
