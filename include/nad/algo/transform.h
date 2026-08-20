#pragma once

#include "nad/algo/fn.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

/*
 * Elementwise mapping into a destination span.
 *
 * Only the LENGTHS have to match here — unlike the rest of algo, the elem
 * sizes need not. Mapping is the one place where the elem type is allowed to
 * change: the op knows both types, and each span is walked with its own
 * stride, so a span of Pair maps into a span of int64_t. That is also why
 * these live apart from copy, which moves bytes as they are.
 *
 * A destination that IS a source is allowed — mapping a span onto itself is
 * the common case — but a partial overlap is not: every position is written
 * before the next is read, so a shifted destination would read what it has
 * already overwritten.
 */

NAD_API
void nad_span_transform(nad_SpanMut dst, nad_Span src, nad_UnOp op, void *ctx);

/// two sources walked in step, so dst[i] = op(a[i], b[i])
NAD_API
void nad_span_zip(nad_SpanMut dst, nad_Span a, nad_Span b, nad_BinOp op, void *ctx);
