#pragma once

#include <stddef.h>
#include <assert.h>

#include "nad/core/export.h"

/// non owning view over a contiguous mem
/// borrows its source and does not extend it
typedef struct {
    const void *data;
    size_t len;
    size_t elem_size;
} nad_Span;

typedef struct {
    void *data;
    size_t len;
    size_t elem_size;
} nad_SpanMut;

#define NAD_SPAN_ASSERT(s)                 \
    (assert((s).data || (s).len == 0), \
     assert((s).elem_size > 0))

/* ========== construction ========== */

/// 'data' may be null only for an empty view
[[nodiscard]] NAD_API
nad_Span nad_span_new(const void *data, size_t len, size_t elem_size);

[[nodiscard]] NAD_API
nad_SpanMut nad_span_new_mut(void *data, size_t len, size_t elem_size);

[[nodiscard]] NAD_API
nad_Span nad_span_from_mut(nad_SpanMut s);

/* ========== subspan ========== */

/// 'start == s.len' is legal and yields and empty view
[[nodiscard]] NAD_API
nad_Span nad_span_sub(nad_Span s, size_t start, size_t count);

[[nodiscard]] NAD_API
nad_SpanMut nad_span_sub_mut(nad_SpanMut s, size_t start, size_t count);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_span_bytes(nad_Span s);

/* ========== access ========== */

[[nodiscard]] NAD_API
const void *nad_span_get(nad_Span s, size_t idx);

[[nodiscard]] NAD_API
void *nad_span_get_mut(nad_SpanMut s, size_t idx);

NAD_API
void nad_span_set(nad_SpanMut s, size_t idx, const void *val);

/* ========== mods ========== */

NAD_API
void nad_span_swap_elems(nad_SpanMut s, size_t i, size_t j);

/* ========== macros ========== */

#define NAD_SPAN_NEW(T, data, len) \
    nad_span_new((data), (len), sizeof(T))

#define NAD_SPAN_NEW_MUT(T, data, len) \
    nad_span_new_mut((data), (len), sizeof(T))

#define NAD_SPAN_GET_AS(T, s, idx) \
    ((const T *) nad_span_get((s), (idx)))

#define NAD_SPAN_GET_MUT_AS(T, s, idx) \
    ((T *) nad_span_get_mut((s), (idx)))

#define NAD_SPAN_SET(T, s, idx, val) \
    nad_span_set((s), (idx), &(T){ (val) })
