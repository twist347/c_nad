#pragma once

#include "nad/core/export.h"
#include "nad/ds/span.h"

#include <stddef.h>

NAD_API
void nad_span_reverse(nad_SpanMut s);

NAD_API
void nad_span_rotate(nad_SpanMut s, size_t mid);
