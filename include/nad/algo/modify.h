#pragma once

#include "nad/algo/fn.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

#include <stddef.h>

/*
 * Operations that change what a span holds, in place.
 *
 * A span cannot resize itself, so the ones that drop elems return the new
 * length instead: the kept elems are packed into the front, and everything
 * from the returned length to s.len is left in an unspecified state. The
 * caller shortens its own container:
 *
 *     const size_t n = nad_span_unique(nad_vec_to_span_mut(v), nad_eq_fn_i32);
 *     nad_Status st = nad_vec_resize(v, n);
 *
 * [[nodiscard]] is what keeps this honest — dropping the new length on the
 * floor, the classic mistake with this family, does not compile.
 */

/* ========== unique ========== */

/// drops every elem equal to the one before it, so a run collapses to its
/// first elem. Only adjacent elems are compared: over a sorted span this
/// leaves a set, over an unsorted one it only collapses runs
[[nodiscard]] NAD_API
size_t nad_span_unique(nad_SpanMut s, nad_Eq eq);

/* ========== remove ========== */

[[nodiscard]] NAD_API
size_t nad_span_remove(nad_SpanMut s, const void *key, nad_Eq eq);

[[nodiscard]] NAD_API
size_t nad_span_remove_if(nad_SpanMut s, nad_Pred pred, void *ctx);

/* ========== replace ========== */

/// overwrites every elem equal to 'key' with 'val'. The length never
/// changes, so there is nothing to return
NAD_API
void nad_span_replace(nad_SpanMut s, const void *key, const void *val, nad_Eq eq);

NAD_API
void nad_span_replace_if(nad_SpanMut s, nad_Pred pred, void *ctx, const void *val);
