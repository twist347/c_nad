#pragma once

#include "nad/core/export.h"
#include "nad/ds/span.h"

NAD_API
void nad_span_copy(nad_SpanMut dst, nad_Span src);

NAD_API
void nad_span_copy_within(nad_SpanMut dst, nad_Span src);
