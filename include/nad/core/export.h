#pragma once

#if defined(NAD_STATIC)
    #define NAD_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(NAD_BUILD)
    #define NAD_API __declspec(dllexport)
#else
    #define NAD_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define NAD_API __attribute__((visibility("default")))
#else
    #define NAD_API
#endif
