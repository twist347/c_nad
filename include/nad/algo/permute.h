#pragma once

#include "nad/core/export.h"
#include "nad/core/cmp.h"
#include "nad/ds/span.h"

#include <stddef.h>

NAD_API
void nad_span_reverse(nad_SpanMut s);

NAD_API
void nad_span_rotate(nad_SpanMut s, size_t mid);

NAD_API
void nad_span_swap_ranges(nad_SpanMut a, nad_SpanMut b);

[[nodiscard]] NAD_API
bool nad_span_next_permutation(nad_SpanMut s, nad_CmpFn cmp);

[[nodiscard]] NAD_API
bool nad_span_prev_permutation(nad_SpanMut s, nad_CmpFn cmp);

// TODO: nad_span_shuffle, nad_span_partition
