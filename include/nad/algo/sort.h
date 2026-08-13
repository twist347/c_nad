#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/status.h"
#include "nad/core/export.h"
#include "nad/ds/span.h"

NAD_API
void nad_span_insertion_sort(nad_SpanMut s, nad_cmp_fn cmp);

NAD_API
void nad_span_sort(nad_SpanMut s, nad_cmp_fn cmp);

[[nodiscard]] NAD_API
nad_Status nad_span_sort_stable(nad_SpanMut s, nad_cmp_fn cmp, nad_Al *al);

NAD_API
void nad_span_partial_sort(nad_SpanMut s, size_t n, nad_cmp_fn cmp);

NAD_API
void nad_span_nth_elem(nad_SpanMut s, size_t nth, nad_cmp_fn cmp);

[[nodiscard]] NAD_API
bool nad_span_is_sorted(nad_Span s, nad_cmp_fn cmp);
