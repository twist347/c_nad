#pragma once

#include "nad/algo/fn.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/ds/span.h"

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
