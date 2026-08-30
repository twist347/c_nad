cmake_minimum_required(VERSION 3.22)

project(nadc
        VERSION 0.1.0
        DESCRIPTION "Naive algorithms and data structures in C23"
        LANGUAGES C
)

if (PROJECT_IS_TOP_LEVEL)
    if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif ()
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_C_EXTENSIONS OFF)
endif ()

set(NAD_C_STANDARD 23)

option(NAD_BUILD_SHARED "Build nadc as a shared library" ${PROJECT_IS_TOP_LEVEL})
option(NAD_BUILD_TEST "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(NAD_BUILD_EXAMPLE "Build the examples the reference quotes" ${PROJECT_IS_TOP_LEVEL})

set(NAD_WARNINGS
        $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
        $<$<C_COMPILER_ID:MSVC>:/W4>
)

set(NAD_ALGO_SOURCES
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

set(NAD_ALLOC_SOURCES
        src/alloc/aligned.c
        src/alloc/alloc.c
        src/alloc/arena.c
        src/alloc/default.c
        src/alloc/log.c
        src/alloc/pool.c
)

set(NAD_CORE_SOURCES
        src/core/cmp.c
        src/core/elem_ops.c
        src/core/hash.c
        src/core/print.c
        src/core/span.c
        src/core/status.c
)

set(NAD_DS_SOURCES
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

set(NAD_SOURCES
        ${NAD_ALGO_SOURCES}
        ${NAD_ALLOC_SOURCES}
        ${NAD_CORE_SOURCES}
        ${NAD_DS_SOURCES}
)

if (NAD_BUILD_SHARED)
    set(NAD_LIBRARY_TYPE SHARED)
else ()
    set(NAD_LIBRARY_TYPE STATIC)
endif ()

add_library(nadc ${NAD_LIBRARY_TYPE} ${NAD_SOURCES})
add_library(nadc::nadc ALIAS nadc)

target_include_directories(nadc
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:include>
        PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(nadc PUBLIC c_std_${NAD_C_STANDARD})
target_compile_options(nadc PRIVATE ${NAD_WARNINGS})

if (UNIX)
    target_link_libraries(nadc PUBLIC m)
endif ()

target_compile_definitions(nadc
        PRIVATE NAD_BUILD
        PUBLIC $<$<NOT:$<BOOL:${NAD_BUILD_SHARED}>>:NAD_STATIC>
)

set_target_properties(nadc PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        POSITION_INDEPENDENT_CODE ON
        C_EXTENSIONS OFF
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

include(CheckIPOSupported)
check_ipo_supported(RESULT NAD_IPO_SUPPORTED OUTPUT NAD_IPO_ERROR)
if (NAD_IPO_SUPPORTED)
    set_target_properties(nadc PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON
    )
else ()
    message(STATUS "  LTO unavailable : ${NAD_IPO_ERROR}")
endif ()

if (NAD_BUILD_TEST OR NAD_BUILD_EXAMPLE)
    enable_testing()
endif ()

if (NAD_BUILD_TEST)
    set(UNITY_DIR ${PROJECT_SOURCE_DIR}/thirdparty/Unity-2.7.0)
    add_subdirectory(${UNITY_DIR} EXCLUDE_FROM_ALL)
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE UNITY_INCLUDE_FLOAT)

    add_subdirectory(test)
endif ()

if (NAD_BUILD_EXAMPLE)
    add_subdirectory(example)
endif ()

if (PROJECT_IS_TOP_LEVEL)
    string(TOLOWER ${NAD_LIBRARY_TYPE} NAD_LIBRARY_TYPE_LOWER)
    message(STATUS "")
    message(STATUS "nadc configuration:")
    message(STATUS "  version        : ${PROJECT_VERSION}")
    message(STATUS "  build type     : ${CMAKE_BUILD_TYPE}")
    message(STATUS "  library        : ${NAD_LIBRARY_TYPE_LOWER}")
    message(STATUS "  compiler       : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
    message(STATUS "  C standard     : C${NAD_C_STANDARD}")
    message(STATUS "  tests          : ${NAD_BUILD_TEST}")
    message(STATUS "  examples       : ${NAD_BUILD_EXAMPLE}")
    message(STATUS "")
endif ()
