#pragma once

#include "nad/algo/fn.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

NAD_API
void nad_span_fill(nad_SpanMut s, const void *val);

NAD_API
void nad_span_fill_zero(nad_SpanMut s);

/// fills every position from 'gen', which receives the index it is filling.
/// A counter kept in 'ctx' turns this into iota; the index alone is enough
/// for anything positional
NAD_API
void nad_span_generate(nad_SpanMut s, nad_Gen gen, void *ctx);
