#pragma once

#include "nad/algo/fn.h"
#include "nad/alloc/alloc.h"
#include "nad/core/export.h"
#include "nad/core/cmp.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

NAD_API
void nad_span_reverse(nad_SpanMut s);

NAD_API
void nad_span_rotate(nad_SpanMut s, size_t mid);

NAD_API
void nad_span_swap_ranges(nad_SpanMut a, nad_SpanMut b);

[[nodiscard]] NAD_API
bool nad_span_next_permutation(nad_SpanMut s, nad_Cmp cmp);

[[nodiscard]] NAD_API
bool nad_span_prev_permutation(nad_SpanMut s, nad_Cmp cmp);

/// moves the elems satisfying 'pred' to the front and reports where they end —
/// the index of the first elem that does not satisfy it. Says nothing about the
/// order within either side: it swaps, and a swap moves two elems at once
[[nodiscard]] NAD_API
size_t nad_span_partition(nad_SpanMut s, nad_Pred pred, void *ctx);

/// the same split, but both sides keep the order they had. That costs a scratch
/// buffer of the whole span — which is why this one is fallible and takes an 'al',
/// exactly like nad_span_sort_stable. 'pred' is asked about each elem once.
/// On failure the span is untouched and '*out_boundary' is not written
[[nodiscard]] NAD_API
nad_Status nad_span_partition_stable(
    nad_SpanMut s,
    nad_Pred pred,
    void *ctx,
    nad_Al *al,
    size_t *out_boundary
);

[[nodiscard]] NAD_API
bool nad_span_is_partitioned(nad_Span s, nad_Pred pred, void *ctx);

// TODO: nad_span_shuffle
