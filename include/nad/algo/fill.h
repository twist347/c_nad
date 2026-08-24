#pragma once

#include "nad/algo/fn.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

/// @file

/// @defgroup algo_fill algo/fill
/// @ingroup algo
/// @brief writing every position of a span
///
/// @par Example
/// @snippet algo/example_fill.c gen
/// @snippet algo/example_fill.c fill
/// @{

/// @name fill
/// @{

/// writes one value over every elem
/// @param s the span
/// @param val the address of the value; elem_size bytes, read once per elem
/// @bigo{n}
NAD_API
void nad_span_fill(nad_SpanMut s, const void *val);

/// zeroes every byte the span spans
/// @param s the span
/// @bigo{n}
NAD_API
void nad_span_fill_zero(nad_SpanMut s);

/// writes every position from 'gen'
/// @param s the span
/// @param gen called per position with the index it fills; a counter in 'ctx' makes this
///            iota, and the index alone covers anything positional
/// @param ctx handed to 'gen'
/// @bigo{n}
NAD_API
void nad_span_generate(nad_SpanMut s, nad_Gen gen, void *ctx);

/// @}

/// @}
