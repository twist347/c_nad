#pragma once

#include "nad/algo/fn.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

#include <stddef.h>

/// @file

/// @defgroup algo_copy algo/copy
/// @ingroup algo
/// @brief moving elems from one span to another, as the bytes they are
///
/// Both spans must have the same elem_size: this copies, it does not convert. Changing
/// the elem type on the way is algo/transform.
///
/// @par Example
/// @snippet algo/example_copy.c pred
/// @snippet algo/example_copy.c copy
/// @{

/// @name copy
/// @{

/// copies 'src' over 'dst'
/// @param dst where they go; asserts the same len and elem_size as 'src'
/// @param src where they come from; no overlap with 'dst' unless it is exactly 'dst'
/// @bigo{n}
NAD_API
void nad_span_copy(nad_SpanMut dst, nad_Span src);

/// copies the elems of 'src' that satisfy 'pred', packed to the front of 'dst'
/// @param dst where they go; asserts room for the ones that pass
/// @param src where they come from
/// @param pred the test
/// @param ctx handed to 'pred'
/// @return how many were written; the rest of 'dst' is untouched
/// @bigo{n}
[[nodiscard]] NAD_API
size_t nad_span_copy_if(nad_SpanMut dst, nad_Span src, nad_Pred pred, void *ctx);

/// nad_span_copy where the two may overlap
/// @param dst where they go; asserts the same len and elem_size as 'src'
/// @param src where they come from, overlapping 'dst' however it likes
/// @bigo{n} — the same copy through memmove instead of memcpy
NAD_API
void nad_span_copy_within(nad_SpanMut dst, nad_Span src);

/// @}

/// @}
