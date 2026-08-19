#pragma once

#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/ds/span.h"

/* ========== equality ========== */

[[nodiscard]] NAD_API
bool nad_span_eq(nad_Span a, nad_Span b);

[[nodiscard]] NAD_API
bool nad_span_eq_by(nad_Span a, nad_Span b, nad_Eq eq);

[[nodiscard]] NAD_API
bool nad_span_mismatch(nad_Span a, nad_Span b, nad_Eq eq, size_t *out_idx);

/* ========== ordering ========== */

[[nodiscard]] NAD_API
int nad_span_cmp(nad_Span a, nad_Span b, nad_Cmp cmp);