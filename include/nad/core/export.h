#pragma once

/// @file

/// @defgroup core_export core/export
/// @ingroup core
/// @brief NAD_API — what marks a symbol as part of the library's surface
///
/// The shared library is built with hidden visibility, so a symbol is exported only by
/// carrying this. Everything else stays inside, which is what lets the internals move
/// without breaking a caller.
///
/// The reference does not show it: doxygen is told to expand it away, since the marker
/// says nothing about what a function does.
///
/// @{

// exported from the shared library, imported by a caller of it, nothing at all under
// NAD_STATIC. Not a /// comment: the reference never shows this macro
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

/// @}
