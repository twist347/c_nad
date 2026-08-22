#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// owning array whose length is fixed when it is built: the elems live in one block,
/// allocated once, and the length is part of what the arr IS rather than something it
/// keeps track of. There is no nad_arr_new — every constructor names a length, because
/// an arr without one is not an empty arr, it is not an arr.
///
/// This is ds/vec minus everything that exists only because a vec can grow: the capacity,
/// the constructors that take one, and every operation that changes a length. Everything
/// else is spelled the same and means the same, so the two are one
/// vocabulary with a boundary drawn through it. The boundary is a TYPE rather than a
/// flag inside one container: what cannot be done is what cannot be named, and no
/// question about it has to be asked at runtime.
///
/// What the fixed length buys is the guarantee the growable containers cannot make: the
/// block never moves under the caller. Pointers from nad_arr_get_mut, views from
/// nad_arr_to_span_mut and the block from nad_arr_data all stay good for as long as the
/// arr does. Only nad_arr_swap breaks that, and it does so by exchanging two arrs whole,
/// lengths and all — the one way a nad_Arr * comes to name a different length, and an
/// operation the caller asks for by name.
///
/// The bridge to algo runs BOTH ways: nad_arr_to_span_mut hands over the elems to sort,
/// partition or fill in place. An arr keeps no order of its own to protect — unlike
/// ds/stack and ds/queue, whose views are read only for exactly that reason.
///
/// An index out of range is a programmer error rather than a runtime state, so get, set
/// and swap_elems assert it. The operations that return a nad_Status here are the ones
/// that allocate.
typedef struct nad_Arr nad_Arr;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Status nad_arr_new_len(size_t len, size_t elem_size, nad_Al *al, nad_Arr **out);

[[nodiscard]] NAD_API
nad_Status nad_arr_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Arr **out);

[[nodiscard]] NAD_API
nad_Status nad_arr_from_span(nad_Span s, nad_Al *al, nad_Arr **out);

NAD_API
void nad_arr_drop(nad_Arr *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API
nad_Status nad_arr_copy(const nad_Arr *self, nad_Arr **out);

[[nodiscard]] NAD_API
nad_Status nad_arr_copy_assign(const nad_Arr *self, nad_Arr *other);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_arr_len(const nad_Arr *self);

[[nodiscard]] NAD_API
size_t nad_arr_elem_size(const nad_Arr *self);

[[nodiscard]] NAD_API
size_t nad_arr_bytes(const nad_Arr *self);

[[nodiscard]] NAD_API
nad_Al *nad_arr_al(const nad_Arr *self);

/* ========== access ========== */

[[nodiscard]] NAD_API
const void *nad_arr_first(const nad_Arr *self);

[[nodiscard]] NAD_API
void *nad_arr_first_mut(nad_Arr *self);

[[nodiscard]] NAD_API
const void *nad_arr_last(const nad_Arr *self);

[[nodiscard]] NAD_API
void *nad_arr_last_mut(nad_Arr *self);

[[nodiscard]] NAD_API
const void *nad_arr_get(const nad_Arr *self, size_t idx);

[[nodiscard]] NAD_API
void *nad_arr_get_mut(nad_Arr *self, size_t idx);

NAD_API
void nad_arr_set(nad_Arr *self, size_t idx, const void *val);

[[nodiscard]] NAD_API
const void *nad_arr_data(const nad_Arr *self);

[[nodiscard]] NAD_API
void *nad_arr_data_mut(nad_Arr *self);

/* ========== mods ========== */

[[nodiscard]] NAD_API
nad_Status nad_arr_swap(nad_Arr *self, nad_Arr *other);

NAD_API
void nad_arr_swap_elems(nad_Arr *self, size_t i, size_t j);

/* ========== to span ========== */

[[nodiscard]] NAD_API
nad_SpanMut nad_arr_to_span_mut(nad_Arr *self);

[[nodiscard]] NAD_API
nad_Span nad_arr_to_span(const nad_Arr *self);

/* ========== print ========== */

NAD_API
void nad_arr_fprint(const nad_Arr *self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_arr_print(const nad_Arr *self, nad_FPrint fprint);

/* ========== macros ========== */

#define NAD_ARR_NEW_LEN(T, len, al, out) \
    nad_arr_new_len((len), sizeof(T), (al), (out))

#define NAD_ARR_FROM_DATA(T, data, len, al, out) \
    nad_arr_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

#define NAD_ARR_OF(T, al, out, ...)                     \
    nad_arr_from_data(                                  \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

#define NAD_ARR_FIRST_AS(T, self) \
    ((const T *) nad_arr_first((self)))

#define NAD_ARR_FIRST_MUT_AS(T, self) \
    ((T *) nad_arr_first_mut((self)))

#define NAD_ARR_LAST_AS(T, self) \
    ((const T *) nad_arr_last((self)))

#define NAD_ARR_LAST_MUT_AS(T, self) \
    ((T *) nad_arr_last_mut((self)))

#define NAD_ARR_GET_AS(T, self, idx) \
    ((const T *) nad_arr_get((self), (idx)))

#define NAD_ARR_GET_MUT_AS(T, self, idx) \
    ((T *) nad_arr_get_mut((self), (idx)))

#define NAD_ARR_SET(T, self, idx, val) \
    nad_arr_set((self), (idx), &(T){ (val) })
