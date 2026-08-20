#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

typedef struct nad_Vec nad_Vec;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Status nad_vec_new(size_t elem_size, nad_Al *al, nad_Vec **out);

[[nodiscard]] NAD_API
nad_Status nad_vec_new_len(size_t len, size_t elem_size, nad_Al *al, nad_Vec **out);

[[nodiscard]] NAD_API
nad_Status nad_vec_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Vec **out);

[[nodiscard]] NAD_API
nad_Status nad_vec_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Vec **out);

[[nodiscard]] NAD_API
nad_Status nad_vec_from_span(nad_Span s, nad_Al *al, nad_Vec **out);

NAD_API
void nad_vec_drop(nad_Vec *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API
nad_Status nad_vec_copy(const nad_Vec *self, nad_Vec **out);

[[nodiscard]] NAD_API
nad_Status nad_vec_copy_assign(const nad_Vec *self, nad_Vec *other);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_vec_len(const nad_Vec *self);

[[nodiscard]] NAD_API
size_t nad_vec_cap(const nad_Vec *self);

[[nodiscard]] NAD_API
size_t nad_vec_elem_size(const nad_Vec *self);

[[nodiscard]] NAD_API
size_t nad_vec_bytes(const nad_Vec *self);

[[nodiscard]] NAD_API
nad_Al *nad_vec_al(const nad_Vec *self);

/* ========== access ========== */

[[nodiscard]] NAD_API
const void *nad_vec_first(const nad_Vec *self);

[[nodiscard]] NAD_API
void *nad_vec_first_mut(nad_Vec *self);

[[nodiscard]] NAD_API
const void *nad_vec_last(const nad_Vec *self);

[[nodiscard]] NAD_API
void *nad_vec_last_mut(nad_Vec *self);

[[nodiscard]] NAD_API
const void *nad_vec_get(const nad_Vec *self, size_t idx);

[[nodiscard]] NAD_API
void *nad_vec_get_mut(nad_Vec *self, size_t idx);

NAD_API
void nad_vec_set(nad_Vec *self, size_t idx, const void *val);

[[nodiscard]] NAD_API
const void *nad_vec_data(const nad_Vec *self);

[[nodiscard]] NAD_API
void *nad_vec_data_mut(nad_Vec *self);

/* ========== mods ========== */

[[nodiscard]] NAD_API
nad_Status nad_vec_push(nad_Vec *self, const void *val);

NAD_API
void nad_vec_pop(nad_Vec *self);

[[nodiscard]] NAD_API
nad_Status nad_vec_insert(nad_Vec *self, size_t idx, const void *val);

NAD_API
void nad_vec_remove(nad_Vec *self, size_t idx);

NAD_API
void nad_vec_clear(nad_Vec *self);

[[nodiscard]] NAD_API
nad_Status nad_vec_reserve(nad_Vec *self, size_t new_cap);

[[nodiscard]] NAD_API
nad_Status nad_vec_shrink_to_fit(nad_Vec *self);

[[nodiscard]] NAD_API
nad_Status nad_vec_resize(nad_Vec *self, size_t new_len);

/// on one allocator the buffers are handed over and the capacity travels with them;
/// on two the bytes are moved and each side is left sized to its new content
[[nodiscard]] NAD_API
nad_Status nad_vec_swap(nad_Vec *self, nad_Vec *other);

NAD_API
void nad_vec_swap_elems(nad_Vec *self, size_t i, size_t j);

/* ========== to span ========== */

[[nodiscard]] NAD_API
nad_SpanMut nad_vec_to_span_mut(nad_Vec *self);

[[nodiscard]] NAD_API
nad_Span nad_vec_to_span(const nad_Vec *self);

/* ========== print ========== */

NAD_API
void nad_vec_fprint(const nad_Vec *self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_vec_print(const nad_Vec *self, nad_FPrint fprint);

/* ========== macros ========== */

#define NAD_VEC_NEW(T, al, out) \
    nad_vec_new(sizeof(T), (al), (out))

#define NAD_VEC_NEW_LEN(T, len, al, out) \
    nad_vec_new_len((len), sizeof(T), (al), (out))

#define NAD_VEC_NEW_CAP(T, cap, al, out) \
    nad_vec_new_cap((cap), sizeof(T), (al), (out))

#define NAD_VEC_FROM_DATA(T, data, len, al, out) \
    nad_vec_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

#define NAD_VEC_OF(T, al, out, ...)                     \
    nad_vec_from_data(                                  \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

#define NAD_VEC_FIRST_AS(T, self) \
    ((const T *) nad_vec_first((self)))

#define NAD_VEC_FIRST_MUT_AS(T, self) \
    ((T *) nad_vec_first_mut((self)))

#define NAD_VEC_LAST_AS(T, self) \
    ((const T *) nad_vec_last((self)))

#define NAD_VEC_LAST_MUT_AS(T, self) \
    ((T *) nad_vec_last_mut((self)))

#define NAD_VEC_GET_AS(T, self, idx) \
    ((const T *) nad_vec_get((self), (idx)))

#define NAD_VEC_GET_MUT_AS(T, self, idx) \
    ((T *) nad_vec_get_mut((self), (idx)))

#define NAD_VEC_SET(T, self, idx, val) \
    nad_vec_set((self), (idx), &(T){ (val) })

#define NAD_VEC_PUSH(T, self, val) \
    nad_vec_push((self), &(T){ (val) })

#define NAD_VEC_INSERT(T, self, idx, val) \
    nad_vec_insert((self), (idx), &(T){ (val) })
