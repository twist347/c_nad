#pragma once

#include "trs/algo/fn.h"
#include "trs/core/cmp.h"
#include "trs/core/export.h"
#include "trs/core/span.h"

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
///     const size_t kept = trs_span_unique(trs_vec_to_span_mut(v), trs_eq_i32);
///     trs_Status st = trs_vec_resize(v, kept);
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
[[nodiscard]] TRS_API
size_t trs_span_unique(trs_SpanMut s, trs_Eq eq);

/// @}

/// @name remove
/// @{

/// drops every elem equal to 'key'
/// @param s the span
/// @param key the address of the value to drop
/// @param eq the equality
/// @return the new length
/// @bigo{n}
[[nodiscard]] TRS_API
size_t trs_span_remove(trs_SpanMut s, const void *key, trs_Eq eq);

/// drops every elem satisfying 'pred'
/// @param s the span
/// @param pred the test
/// @param ctx handed to 'pred'
/// @return the new length
/// @bigo{n}
[[nodiscard]] TRS_API
size_t trs_span_remove_if(trs_SpanMut s, trs_Pred pred, void *ctx);

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
TRS_API
void trs_span_replace(trs_SpanMut s, const void *key, const void *val, trs_Eq eq);

/// overwrites every elem satisfying 'pred' with 'val'
/// @param s the span
/// @param pred the test
/// @param ctx handed to 'pred'
/// @param val the address of the value to write
/// @bigo{n}
TRS_API
void trs_span_replace_if(trs_SpanMut s, trs_Pred pred, void *ctx, const void *val);

/// @}

/// @}
