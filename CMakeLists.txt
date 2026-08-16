cmake_minimum_required(VERSION 3.22)

project(nadc LANGUAGES C)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif ()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NAD_ALGO_SOURCES
        src/algo/compare.c
        src/algo/copy.c
        src/algo/fill.c
        src/algo/merge.c
        src/algo/permute.c
        src/algo/search.c
        src/algo/sort.c
)

set(NAD_ALLOC_SOURCES
        src/alloc/alloc.c
        src/alloc/alloc_arena.c
        src/alloc/alloc_default.c
        src/alloc/alloc_log.c
        src/alloc/alloc_pool.c
)

set(NAD_CORE_SOURCES
        src/core/cmp.c
        src/core/elem_ops.c
        src/core/status.c
)

set(NAD_DS_SOURCES
        src/ds/arr.c
        src/ds/list.c
        src/ds/span.c
        src/ds/vec.c
)

set(NAD_SOURCES
        ${NAD_ALGO_SOURCES}
        ${NAD_ALLOC_SOURCES}
        ${NAD_CORE_SOURCES}
        ${NAD_DS_SOURCES}
)

add_library(nadc_api INTERFACE)
target_include_directories(nadc_api INTERFACE ${PROJECT_SOURCE_DIR}/include)
set(NAD_C_STANDARD 23)
target_compile_features(nadc_api INTERFACE c_std_${NAD_C_STANDARD})
target_link_libraries(nadc_api INTERFACE m)

option(BUILD_SHARED_LIBS "Build nadc as a shared library" ON)

add_library(nadc ${NAD_SOURCES})
add_library(nadc::nadc ALIAS nadc)
target_link_libraries(nadc PUBLIC nadc_api)
target_compile_options(nadc PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(nadc PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_compile_definitions(nadc
        PRIVATE NAD_BUILD
        PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:NAD_STATIC>
)
set_target_properties(nadc PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_EXTENSIONS OFF
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

# Release only. The algo modules reach into ds/span for every element access, so without
# cross-TU inlining those stay real calls; with it the sort loses about a third of its
# time. Debug keeps it off — it slows linking and blurs the code the debugger shows.
include(CheckIPOSupported)
check_ipo_supported(RESULT NAD_IPO_SUPPORTED OUTPUT NAD_IPO_ERROR)
if (NAD_IPO_SUPPORTED)
    set_property(TARGET nadc PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else ()
    message(STATUS "  LTO unavailable : ${NAD_IPO_ERROR}")
endif ()

option(NAD_BUILD_TEST "Build unit tests" ${PROJECT_IS_TOP_LEVEL})

if (NAD_BUILD_TEST)
    set(UNITY_DIR ${PROJECT_SOURCE_DIR}/thirdparty/Unity-2.7.0)
    add_subdirectory(${UNITY_DIR} EXCLUDE_FROM_ALL)
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE UNITY_INCLUDE_FLOAT)

    enable_testing()
    add_subdirectory(test)
endif ()

if (BUILD_SHARED_LIBS)
    set(NAD_LIBRARY_TYPE "shared")
else ()
    set(NAD_LIBRARY_TYPE "static")
endif ()

message(STATUS "")
message(STATUS "nadc configuration:")
message(STATUS "  build type     : ${CMAKE_BUILD_TYPE}")
message(STATUS "  library        : ${NAD_LIBRARY_TYPE}")
message(STATUS "  compiler       : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  C standard     : C${NAD_C_STANDARD}")
message(STATUS "  tests          : ${NAD_BUILD_TEST}")
message(STATUS "")
