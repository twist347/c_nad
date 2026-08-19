#pragma once

#include "nad/algo/fn.h"
#include "nad/core/export.h"
#include "nad/ds/span.h"

#include <stddef.h>

NAD_API
void nad_span_copy(nad_SpanMut dst, nad_Span src);

[[nodiscard]] NAD_API
size_t nad_span_copy_if(nad_SpanMut dst, nad_Span src, nad_Pred pred, void *ctx);

NAD_API
void nad_span_copy_within(nad_SpanMut dst, nad_Span src);
