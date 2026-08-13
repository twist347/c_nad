#pragma once

#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/ds/span.h"

NAD_API
void nad_span_merge(nad_SpanMut dst, nad_Span a, nad_Span b, nad_cmp_fn cmp);
