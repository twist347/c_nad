#pragma once

#include "nad/core/export.h"
#include "nad/core/print.h"

#include <stddef.h>
#include <assert.h>

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

#define NAD_SPAN_ASSERT(s)             \
    (assert((s).data || (s).len == 0), \
     assert((s).elem_size > 0))

/* ========== construction ========== */

/// 'data' may be null only for an empty view
[[nodiscard]] NAD_API
nad_Span nad_span_new(const void *data, size_t len, size_t elem_size);

[[nodiscard]] NAD_API
nad_SpanMut nad_span_new_mut(void *data, size_t len, size_t elem_size);

/* ========== to span ========== */

/// the same memory, seen read only. There is no way back: a view cannot gain mutability
[[nodiscard]] NAD_API
nad_Span nad_span_mut_to_span(nad_SpanMut s);

/* ========== subspan ========== */

/// 'start == s.len' is legal and yields and empty view
[[nodiscard]] NAD_API
nad_Span nad_span_sub(nad_Span self, size_t start, size_t count);

[[nodiscard]] NAD_API
nad_SpanMut nad_span_sub_mut(nad_SpanMut self, size_t start, size_t count);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_span_bytes(nad_Span self);

/* ========== access ========== */

[[nodiscard]] NAD_API
const void *nad_span_get(nad_Span self, size_t idx);

[[nodiscard]] NAD_API
void *nad_span_get_mut(nad_SpanMut self, size_t idx);

NAD_API
void nad_span_set(nad_SpanMut self, size_t idx, const void *val);

/* ========== mods ========== */

NAD_API
void nad_span_swap_elems(nad_SpanMut self, size_t i, size_t j);

/* ========== print ========== */

NAD_API
void nad_span_fprint(nad_Span self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_span_mut_fprint(nad_SpanMut self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_span_print(nad_Span self, nad_FPrint fprint);

NAD_API
void nad_span_mut_print(nad_SpanMut self, nad_FPrint fprint);

/* ========== macros ========== */

#define NAD_SPAN_NEW(T, data, len) \
    nad_span_new((const T *){ (data) }, (len), sizeof(T))

#define NAD_SPAN_NEW_MUT(T, data, len) \
    nad_span_new_mut((T *){ (data) }, (len), sizeof(T))

/// view over a compound literal — it lives until the end of the enclosing block,
/// so the view must not outlive it
#define NAD_SPAN_OF(T, ...)                             \
    nad_span_new(                                       \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T))

#define NAD_SPAN_OF_MUT(T, ...)                   \
    nad_span_new_mut(                             \
        (T[]){ __VA_ARGS__ },                     \
        sizeof((T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T))

#define NAD_SPAN_GET_AS(T, s, idx) \
    ((const T *) nad_span_get((s), (idx)))

#define NAD_SPAN_GET_MUT_AS(T, s, idx) \
    ((T *) nad_span_get_mut((s), (idx)))

#define NAD_SPAN_SET(T, s, idx, val) \
    nad_span_set((s), (idx), &(T){ (val) })
