#pragma once

#include "nad/algo/fn.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

/// @file

/// @defgroup algo_fold algo/fold
/// @ingroup algo
/// @brief folding a span into one value, and keeping the steps
///
/// The accumulator is the caller's and keeps the caller's type, so a span of int32_t
/// folds into an int64_t, a count, or a struct. An empty span leaves it untouched, which
/// makes the initial value the identity of the operation. A scan keeps what a fold throws
/// away: every intermediate result.
///
/// @par Example
/// @snippet algo/example_fold.c ops
/// @snippet algo/example_fold.c fold
/// @{

/// @name fold
/// @{

/// folds the span into 'acc', front to back
/// @param s the span
/// @param[in,out] acc the accumulator, the caller's own and of the caller's own type
/// @param fold called once per elem
/// @param ctx handed to 'fold'
/// @bigo{n}
NAD_API
void nad_span_fold(nad_Span s, void *acc, nad_Fold fold, void *ctx);

/// folds the span into 'acc', back to front
/// @param s the span
/// @param[in,out] acc the accumulator, the caller's own and of the caller's own type
/// @param fold called once per elem
/// @param ctx handed to 'fold'
/// @bigo{n}
NAD_API
void nad_span_rfold(nad_Span s, void *acc, nad_Fold fold, void *ctx);

/// @}

/// @name scan
/// @{

/// running totals: dst[0] = src[0], then dst[i] = op(dst[i - 1], src[i])
/// @param dst where they go; asserts the same len as 'src' and no overlap with it — a
///            scan reads past what it has written, and would feed itself its own output
/// @param src where they come from
/// @param op what combines a running result with the next elem
/// @param ctx handed to 'op'
/// @bigo{n}
NAD_API
void nad_span_partial_sum(nad_SpanMut dst, nad_Span src, nad_BinOp op, void *ctx);

/// steps between neighbours: dst[0] = src[0], then dst[i] = op(src[i], src[i - 1])
/// @param dst where they go; asserts the same len as 'src', and must not overlap it
/// @param src where they come from
/// @param op what combines an elem with the one before it; the inverse of
///           nad_span_partial_sum when 'op' is the inverse of its op
/// @param ctx handed to 'op'
/// @bigo{n}
NAD_API
void nad_span_adjacent_difference(nad_SpanMut dst, nad_Span src, nad_BinOp op, void *ctx);

/// @}

/// @}
