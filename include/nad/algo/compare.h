#pragma once

#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

/// @file

/// @defgroup algo_compare algo/compare
/// @ingroup algo
/// @brief comparing two spans against each other
///
/// The operands are 'a' and 'b', not a receiver and an argument — neither is the one
/// being asked about. Same elem_size for both; differing lengths are each function's own
/// business.
///
/// @par Example
/// @snippet algo/example_compare.c compare
/// @{

/// @name equality
/// @{

/// whether the two hold the same bytes
/// @param a one span
/// @param b the other
/// @return whether the lengths match and the bytes do — being memcmp, it parts -0.0 from
///         +0.0 where nad_eq_f32 would not, and a struct's padding counts
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_span_eq(nad_Span a, nad_Span b);

/// whether the two hold equal elems under 'eq'
/// @param a one span
/// @param b the other
/// @param eq the equality
/// @return whether the lengths match and every pair does
/// @bigo{n} — it stops at the first pair that does not
[[nodiscard]] NAD_API
bool nad_span_eq_by(nad_Span a, nad_Span b, nad_Eq eq);

/// where the two first differ
/// @param a one span
/// @param b the other
/// @param eq the equality
/// @param[out] out_idx the first index where they differ, written only when they do
/// @return whether they differ anywhere in their common prefix
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_span_mismatch(nad_Span a, nad_Span b, nad_Eq eq, size_t *out_idx);

/// @}

/// @name ordering
/// @{

/// orders the two the way a dictionary orders words
/// @param a one span
/// @param b the other
/// @param cmp the order elems are in
/// @return negative, zero or positive as 'a' orders before, with, or after 'b': the first
///         differing pair decides, failing which the shorter orders first
/// @bigo{n}
[[nodiscard]] NAD_API
int nad_span_cmp(nad_Span a, nad_Span b, nad_Cmp cmp);

/// @}

/// @}
