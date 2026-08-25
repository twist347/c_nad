#pragma once

#include "nad/algo/fn.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

/// @file

/// @defgroup algo_transform algo/transform
/// @ingroup algo
/// @brief mapping a span elemwise into another
///
/// Only the lengths have to match: unlike the rest of algo the elem sizes need not, since
/// the op knows both types and each span is walked with its own stride — a span of Pair
/// maps into a span of int64_t. algo/copy, which moves bytes as they are, is the other
/// half of that line.
///
/// A destination that is a source is fine; a partial overlap is not, since every position
/// is written before the next is read.
///
/// @par Example
/// @snippet algo/example_transform.c ops
/// @snippet algo/example_transform.c map
/// @{

/// @name transform
/// @{

/// writes op(src[i]) at dst[i]
/// @param dst where the results go; asserts the same len as 'src'
/// @param src where the operands come from
/// @param op what to apply, called once per position
/// @param ctx handed to 'op'
/// @bigo{n}
NAD_API
void nad_span_transform(nad_SpanMut dst, nad_Span src, nad_UnOp op, void *ctx);

/// writes op(a[i], b[i]) at dst[i], the two sources walked in step
/// @param dst where the results go; asserts the same len as both sources
/// @param a one source
/// @param b the other
/// @param op what to apply, called once per position
/// @param ctx handed to 'op'
/// @bigo{n}
NAD_API
void nad_span_zip(nad_SpanMut dst, nad_Span a, nad_Span b, nad_BinOp op, void *ctx);

/// @}

/// @}
