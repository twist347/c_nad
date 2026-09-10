#pragma once

/// @file

/// @defgroup core_export core/export
/// @ingroup core
/// @brief TRS_API — what marks a symbol as part of the library's surface
///
/// The shared library is built with hidden visibility, so a symbol is exported only by
/// carrying this. Everything else stays inside, which is what lets the internals move
/// without breaking a caller.
///
/// The reference does not show it: doxygen is told to expand it away.
///
/// @{

// exported from the shared library, imported by a caller of it, nothing at all under
// TRS_STATIC. Not a /// comment: the reference never shows this macro
#if defined(TRS_STATIC)
    #define TRS_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(TRS_BUILD)
    #define TRS_API __declspec(dllexport)
#else
    #define TRS_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define TRS_API __attribute__((visibility("default")))
#else
    #define TRS_API
#endif

/// @}
