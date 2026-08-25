#pragma once

#include "nad/algo/fn.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

#include <stddef.h>

/// @file

/// @defgroup algo_modify algo/modify
/// @ingroup algo
/// @brief changing what a span holds, in place
///
/// A span cannot resize itself, so the ops that drop elems return the new length instead:
/// the kept elems are packed to the front, and everything from there to s.len is left in
/// an unspecified state. The caller shortens its own container:
///
///     const size_t kept = nad_span_unique(nad_vec_to_span_mut(v), nad_eq_i32);
///     nad_Status st = nad_vec_resize(v, kept);
///
/// [[nodiscard]] makes dropping that length a compile error.
///
/// @par Example
/// @snippet algo/example_modify.c drop
/// @{

/// @name unique
/// @{

/// drops every elem equal to the one before it, so a run collapses to its first
/// @param s the span
/// @param eq the equality
/// @return the new length. Only neighbours are compared, so this leaves a set over a
///         sorted span and merely collapses runs over any other
/// @bigo{n}
[[nodiscard]] NAD_API
size_t nad_span_unique(nad_SpanMut s, nad_Eq eq);

/// @}

/// @name remove
/// @{

/// drops every elem equal to 'key'
/// @param s the span
/// @param key the address of the value to drop
/// @param eq the equality
/// @return the new length
/// @bigo{n}
[[nodiscard]] NAD_API
size_t nad_span_remove(nad_SpanMut s, const void *key, nad_Eq eq);

/// drops every elem satisfying 'pred'
/// @param s the span
/// @param pred the test
/// @param ctx handed to 'pred'
/// @return the new length
/// @bigo{n}
[[nodiscard]] NAD_API
size_t nad_span_remove_if(nad_SpanMut s, nad_Pred pred, void *ctx);

/// @}

/// @name replace
/// @{

/// overwrites every elem equal to 'key' with 'val'
/// @param s the span
/// @param key the address of the value to look for
/// @param val the address of the value to write; the length never changes, so there is
///            nothing to return
/// @param eq the equality
/// @bigo{n}
NAD_API
void nad_span_replace(nad_SpanMut s, const void *key, const void *val, nad_Eq eq);

/// overwrites every elem satisfying 'pred' with 'val'
/// @param s the span
/// @param pred the test
/// @param ctx handed to 'pred'
/// @param val the address of the value to write
/// @bigo{n}
NAD_API
void nad_span_replace_if(nad_SpanMut s, nad_Pred pred, void *ctx, const void *val);

/// @}

/// @}
