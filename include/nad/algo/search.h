#pragma once

#include "nad/algo/fn.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

/* ========== search ========== */

[[nodiscard]] NAD_API
bool nad_span_find(nad_Span s, const void *key, nad_Eq eq, size_t *out_idx);

[[nodiscard]] NAD_API
bool nad_span_find_if(nad_Span s, nad_Pred pred, void *ctx, size_t *out_idx);

[[nodiscard]] NAD_API
bool nad_span_contains(nad_Span s, const void *key, nad_Eq eq);

[[nodiscard]] NAD_API
size_t nad_span_count(nad_Span s, const void *key, nad_Eq eq);

[[nodiscard]] NAD_API
size_t nad_span_count_if(nad_Span s, nad_Pred pred, void *ctx);

[[nodiscard]] NAD_API
size_t nad_span_lower_bound(nad_Span s, const void *key, nad_Cmp cmp);

[[nodiscard]] NAD_API
size_t nad_span_upper_bound(nad_Span s, const void *key, nad_Cmp cmp);

[[nodiscard]] NAD_API
bool nad_span_bsearch(nad_Span s, const void *key, nad_Cmp cmp, size_t *out_idx);

/// [lo, hi)
typedef struct {
    size_t lo;
    size_t hi;
} nad_Range;

[[nodiscard]] NAD_API
nad_Range nad_span_equal_range(nad_Span s, const void *key, nad_Cmp cmp);

/// the boundary of a span already partitioned by 'pred' — the index of the
/// first elem that does not satisfy it, or s.len when they all do. Being a
/// binary search, it needs 'pred' to be monotonic over the span: true for
/// a prefix, false for the rest. Over a sorted span with "less than key"
/// this is exactly nad_span_lower_bound
[[nodiscard]] NAD_API
size_t nad_span_partition_point(nad_Span s, nad_Pred pred, void *ctx);

/* ========== predicates ========== */

[[nodiscard]] NAD_API
bool nad_span_all_of(nad_Span s, nad_Pred pred, void *ctx);

[[nodiscard]] NAD_API
bool nad_span_any_of(nad_Span s, nad_Pred pred, void *ctx);

[[nodiscard]] NAD_API
bool nad_span_none_of(nad_Span s, nad_Pred pred, void *ctx);

/* ========== extremes ========== */

[[nodiscard]] NAD_API
size_t nad_span_min_elem(nad_Span s, nad_Cmp cmp);

[[nodiscard]] NAD_API
size_t nad_span_max_elem(nad_Span s, nad_Cmp cmp);

typedef struct {
    size_t min;
    size_t max;
} nad_MinMax;

[[nodiscard]] NAD_API
nad_MinMax nad_span_minmax_elem(nad_Span s, nad_Cmp cmp);
