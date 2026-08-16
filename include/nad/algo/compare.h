#pragma once

#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/ds/span.h"

[[nodiscard]] NAD_API
bool nad_span_eq(nad_Span a, nad_Span b);

[[nodiscard]] NAD_API
bool nad_span_eq_by(nad_Span a, nad_Span b, nad_Eq eq);
