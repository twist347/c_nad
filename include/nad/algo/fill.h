#pragma once

#include "nad/core/export.h"
#include "nad/ds/span.h"

NAD_API
void nad_span_fill(nad_SpanMut s, const void *val);

NAD_API
void nad_span_fill_zero(nad_SpanMut s);
