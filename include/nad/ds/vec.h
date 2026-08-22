#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// owning growable array of type-erased elems: one contiguous block, a length that says
/// how much of it holds elems and a capacity that says how much of it there is. Pushing
/// costs O(1) amortized — the block doubles when it fills, so across a run of pushes each
/// elem is moved a constant number of times on average.
///
/// Those two numbers are the whole difference from ds/arr, which has a length and nothing
/// else. Everything an arr does a vec does and spells the same way; what a vec adds is
/// what only means something when the block can change size — the capacity, the
/// constructors that take one, and every operation that changes a length.
///
/// Growing moves the elems. A pointer from nad_vec_get_mut, a view from
/// nad_vec_to_span_mut and the block from nad_vec_data are good only until the next
/// operation that may reallocate — push, insert, extend, insert_span, reserve, resize,
/// shrink_to_fit or swap. Stable positions are what ds/list is for.
///
/// The bridge to algo runs BOTH ways: nad_vec_to_span_mut hands the elems over to be
/// sorted, partitioned or filled in place, and nad_vec_resize adopts whatever length the
/// algorithm leaves behind — the pair the header of algo/modify is written around. A vec
/// keeps no order of its own to protect, unlike ds/stack and ds/queue.
///
/// An index out of range is a programmer error rather than a runtime state, so get, set
/// and swap_elems assert it. The operations that return a nad_Status are the ones that
/// allocate.
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

/// 'val' must not point into this vec's own buffer: a push that grows moves the elems
/// out from under it
[[nodiscard]] NAD_API
nad_Status nad_vec_push(nad_Vec *self, const void *val);

NAD_API
void nad_vec_pop(nad_Vec *self);

/// 'idx == len' is legal and means push. 'val' must not point into this vec's own
/// buffer, for the same reason it must not in push
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

/* ========== bulk mods ========== */

/// appends every elem of 'src' to the back, in order. The room is taken once for the
/// whole run rather than once per elem, and taken with the same growth factor a push
/// uses, so a run of extends stays amortized O(1) per elem.
///
/// 'src' must not view this vec's own buffer: growing frees the block 'src' would be
/// reading from. This is the rule nad_span_copy holds callers to, for the same reason
[[nodiscard]] NAD_API
nad_Status nad_vec_extend(nad_Vec *self, nad_Span src);

/// inserts every elem of 'src' before 'idx', in order; 'idx == len' is legal and means
/// extend. The tail moves ONCE for the whole run, which is what a loop of insert cannot
/// do: that loop moves the tail once per elem and costs O(len * src.len).
///
/// 'src' must not view this vec's own buffer, as in extend
[[nodiscard]] NAD_API
nad_Status nad_vec_insert_span(nad_Vec *self, size_t idx, nad_Span src);

/// drops 'count' elems starting at 'idx' and closes the gap, moving the tail once.
/// 'count == 0' is legal and does nothing. Nothing is allocated, so nothing can fail
NAD_API
void nad_vec_remove_range(nad_Vec *self, size_t idx, size_t count);

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

#define NAD_VEC_EXTEND(T, self, ...) \
    nad_vec_extend((self), NAD_SPAN_OF(T, __VA_ARGS__))

#define NAD_VEC_INSERT_SPAN(T, self, idx, ...) \
    nad_vec_insert_span((self), (idx), NAD_SPAN_OF(T, __VA_ARGS__))
