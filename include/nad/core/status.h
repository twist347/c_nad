#pragma once

#include "nad/core/export.h"

#include <stdint.h>

/// @file

/// @defgroup core_status core/status
/// @ingroup core
/// @brief nad_Status — the one way an operation reports that it failed
///
/// A fallible op returns a nad_Status and writes its result through a trailing 'out',
/// which it touches only on NAD_STATUS_OK. The return is [[nodiscard]], and C has no
/// defer, so an error is propagated by hand: a return when nothing is held yet, a goto
/// when something is.
///
/// The inverse shape — the value returned, the status written out — is deliberately
/// absent: a status out-param is ignorable. The alloc wrappers are the one exception,
/// saying failure with a null pointer.
///
/// A broken precondition asserts instead: a null self, an index past the end, an
/// elem_size of 0. A nad_Status is for what valid code can still meet at runtime, and
/// that is memory — there is no enumerator for a bad argument.
///
/// @par Example
/// @snippet core/example_status.c propagate
/// @snippet core/example_status.c report
/// @{

/// How an operation reports its outcome. Fixed at int32_t, so the value crosses an ABI
/// boundary at a width that does not depend on the compiler.
typedef enum : int32_t {
    NAD_STATUS_OK = 0,         ///< success — the only value on which an 'out' is written
    NAD_STATUS_ERR_NO_MEM,     ///< an allocation failed, or the size asked for overflowed
} nad_Status;

/// @name to str
/// @{

/// the status spelled as it is written in this header
/// @param st the status
/// @return a static string — "NAD_STATUS_OK" and so on, or "UNKNOWN_NAD_STATUS" for a
///         value that is none of the enumerators. Never null, and never the caller's to
///         free
/// @bigo{1}
[[nodiscard]] NAD_API
const char *nad_status_to_str(nad_Status st);

/// @}

/// @name macros
/// @{

/// whether the op succeeded
/// @param st the status, evaluated once
/// @bigo{1}
#define NAD_STATUS_IS_OK(st)    ((st) == NAD_STATUS_OK)

/// whether the op failed — the form the library's own call sites use, since a caller
/// acts on failure and falls through on success
/// @param st the status, evaluated once
/// @bigo{1}
#define NAD_STATUS_IS_ERR(st)   ((st) != NAD_STATUS_OK)

/// @}

/// @}
