#pragma once

#include "nad/algo/fn.h"
#include "nad/core/export.h"
#include "nad/ds/span.h"

/*
 * Left fold over a span.
 *
 * The accumulator lives in the caller and keeps its own type, so a span of
 * int32_t can fold into an int64_t sum, into a count, or into a struct. The
 * span is walked front to back, and an empty span leaves 'acc' untouched —
 * which makes the caller's initial value the identity of the operation.
 */

NAD_API
void nad_span_fold(nad_Span s, void *acc, nad_Fold fold, void *ctx);

NAD_API
void nad_span_rfold(nad_Span s, void *acc, nad_Fold fold, void *ctx);

/* ========== scan ========== */

/*
 * A scan keeps what a fold throws away: every intermediate result. Both write
 * one elem per elem of 'src', so 'dst' must be exactly as long, and neither
 * allows 'dst' to overlap 'src' — they read positions they have already
 * written past, so an overlap would feed them their own output. Running a
 * scan in place means scanning into a separate span and copying back.
 */

/// dst[0] = src[0], dst[i] = op(dst[i - 1], src[i]) — running totals
NAD_API
void nad_span_partial_sum(nad_SpanMut dst, nad_Span src, nad_BinOp op, void *ctx);

/// dst[0] = src[0], dst[i] = op(src[i], src[i - 1]) — the inverse of
/// partial_sum when 'op' is the inverse of its op
NAD_API
void nad_span_adjacent_difference(nad_SpanMut dst, nad_Span src, nad_BinOp op, void *ctx);
