#pragma once

#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/ds/span.h"

/* ========== sort ========== */

NAD_API
void nad_span_insertion_sort(nad_SpanMut s, nad_cmp_fn cmp);

/* ========== info ========== */

[[nodiscard]] NAD_API
bool nad_span_is_sorted(nad_Span s, nad_cmp_fn cmp);
