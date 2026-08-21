#pragma once

#include "nad/algo/fn.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

/*
 * Two verbs, and the choice between them is not a matter of taste: 'find' is a linear
 * scan valid over any span, 'search' is a binary descent that needs the span sorted by
 * the same cmp. So nad_span_binary_search is the only 'search' here, and every
 * pattern-matching op is a 'find' no matter what the standard library calls it.
 *
 * The suffix says WHAT is looked for, never from which end: scanning backwards is
 * '_last', not a different verb.
 *
 * The boundary ops (lower_bound, upper_bound, equal_range, partition_point) are binary
 * too but carry neither verb — they answer "where", not "is it there".
 *
 * Every find reports through 'out_idx' and returns whether it hit: not found is an
 * answer, not an error. 'out_idx' is left untouched on a miss.
 */

/* ========== find ========== */

[[nodiscard]] NAD_API
bool nad_span_find(nad_Span s, const void *key, nad_Eq eq, size_t *out_idx);

[[nodiscard]] NAD_API
bool nad_span_find_if(nad_Span s, nad_Pred pred, void *ctx, size_t *out_idx);

/// the first index at which 'sub' occurs in 's', elem by elem under 'eq'. An empty
/// 'sub' occurs at 0. O(n*m): a plain scan, no preprocessing
[[nodiscard]] NAD_API
bool nad_span_find_sub(nad_Span s, nad_Span sub, nad_Eq eq, size_t *out_idx);

/// the LAST index at which 'sub' occurs in 's'. An empty 'sub' occurs at s.len
[[nodiscard]] NAD_API
bool nad_span_find_sub_last(nad_Span s, nad_Span sub, nad_Eq eq, size_t *out_idx);

/// the start of the first run of 'n' consecutive elems equal to 'key'.
/// A run of 0 is found at 0
[[nodiscard]] NAD_API
bool nad_span_find_run(nad_Span s, const void *key, size_t n, nad_Eq eq, size_t *out_idx);

/// the first index in 's' whose elem equals any elem of 'set', which needs no order and
/// may hold duplicates. O(n*k)
[[nodiscard]] NAD_API
bool nad_span_find_any_of(nad_Span s, nad_Span set, nad_Eq eq, size_t *out_idx);

/// the index of the first elem equal to the one right after it — the start of the
/// first adjacent equal pair
[[nodiscard]] NAD_API
bool nad_span_find_adjacent(nad_Span s, nad_Eq eq, size_t *out_idx);

[[nodiscard]] NAD_API
bool nad_span_contains(nad_Span s, const void *key, nad_Eq eq);

/* ========== count ========== */

[[nodiscard]] NAD_API
size_t nad_span_count(nad_Span s, const void *key, nad_Eq eq);

[[nodiscard]] NAD_API
size_t nad_span_count_if(nad_Span s, nad_Pred pred, void *ctx);

/* ========== binary search ========== */

[[nodiscard]] NAD_API
size_t nad_span_lower_bound(nad_Span s, const void *key, nad_Cmp cmp);

[[nodiscard]] NAD_API
size_t nad_span_upper_bound(nad_Span s, const void *key, nad_Cmp cmp);

[[nodiscard]] NAD_API
bool nad_span_binary_search(nad_Span s, const void *key, nad_Cmp cmp, size_t *out_idx);

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
