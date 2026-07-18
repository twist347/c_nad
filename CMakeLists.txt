cmake_minimum_required(VERSION 3.22)

project(nadс LANGUAGES C)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif ()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NAD_CORE_SOURCES
        src/core/status.c
)

set(NAD_DS_SOURCES
        src/ds/arr.c
)

set(NAD_MEM_SOURCES
        src/mem/alloc.c
        src/mem/alloc_default.c
)

set(NAD_SOURCES
        ${NAD_CORE_SOURCES}
        ${NAD_DS_SOURCES}
        ${NAD_MEM_SOURCES}
)

add_library(nadc_api INTERFACE)
target_include_directories(nadc_api INTERFACE ${PROJECT_SOURCE_DIR}/include)
set(NAD_C_STANDARD 23)
target_compile_features(nadc_api INTERFACE c_std_${NAD_C_STANDARD})
target_link_libraries(nadc_api INTERFACE m)

option(BUILD_SHARED_LIBS "Build nadс as a shared library" ON)

add_library(nadc ${NAD_SOURCES})
add_library(nadc::nadc ALIAS nadc)
target_link_libraries(nadc PUBLIC nadc_api)
target_compile_options(nadc PRIVATE -Wall -Wextra -pedantic)

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