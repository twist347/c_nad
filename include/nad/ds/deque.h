#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// owning double ended queue: a growable ring buffer over one block, so both ends cost
/// O(1) amortized and get by index stays O(1) — the elem for 'idx' sits at
/// '(head + idx) % cap'.
///
/// The contents may therefore WRAP: they are one run of elems in ring order, not one run
/// of bytes. Two things follow, and both are absent on purpose rather than missing.
///
/// There is no nad_deque_data: there is no single block to point at.
///
/// There is no nad_deque_to_span either, and no linearize to earn one. Making the ring
/// contiguous would be a mutation whose result the next push_front undoes, i.e. a public
/// function leaving the deque in a state nothing can rely on. The bridge to algo is the
/// pair nad_deque_copy_to_span / nad_deque_copy_from_span, which copies instead of
/// rearranging — a copy is what algo needs anyway, and taking it costs a const deque
/// nothing but time.
///
/// Nor are the pointers stable: growing moves every elem. That is what ds/list is for.
typedef struct nad_Deque nad_Deque;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Status nad_deque_new(size_t elem_size, nad_Al *al, nad_Deque **out);

/// 'len' zeroed elems, ready to be written through get_mut/set
[[nodiscard]] NAD_API
nad_Status nad_deque_new_len(size_t len, size_t elem_size, nad_Al *al, nad_Deque **out);

[[nodiscard]] NAD_API
nad_Status nad_deque_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Deque **out);

/// the elems land in order, front to back, in a ring that starts out unwrapped
[[nodiscard]] NAD_API
nad_Status nad_deque_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Deque **out);

[[nodiscard]] NAD_API
nad_Status nad_deque_from_span(nad_Span s, nad_Al *al, nad_Deque **out);

NAD_API
void nad_deque_drop(nad_Deque *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API
nad_Status nad_deque_copy(const nad_Deque *self, nad_Deque **out);

[[nodiscard]] NAD_API
nad_Status nad_deque_copy_assign(const nad_Deque *self, nad_Deque *other);

/// writes every elem into 'dst' in ring order — at most two memcpy, since the contents
/// are at most two runs. 'dst' must be exactly as long as the deque, the same rule
/// nad_span_copy holds callers to. This is how a deque reaches algo: sort or search the
/// copy, not the ring
NAD_API
void nad_deque_copy_to_span(const nad_Deque *self, nad_SpanMut dst);

/// overwrites every elem from 'src', which must be exactly as long as the deque. The pair
/// to nad_deque_copy_to_span: take the contents out, hand them to algo, put the answer
/// back. Nothing is allocated, so nothing can fail
NAD_API
void nad_deque_copy_from_span(nad_Deque *self, nad_Span src);

/* ========== compare ========== */

/// whether the two hold the same elems in ring order, byte for byte. Same elem_size for both —
/// a mismatch there is a programmer error, not a false; differing lengths are just false
[[nodiscard]] NAD_API
bool nad_deque_eq(const nad_Deque *a, const nad_Deque *b);

/// whether the two hold equal elems under 'eq', which is asked of every pair until one
/// says no
[[nodiscard]] NAD_API
bool nad_deque_eq_by(const nad_Deque *a, const nad_Deque *b, nad_Eq eq);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_deque_len(const nad_Deque *self);

[[nodiscard]] NAD_API
size_t nad_deque_cap(const nad_Deque *self);

[[nodiscard]] NAD_API
size_t nad_deque_elem_size(const nad_Deque *self);

[[nodiscard]] NAD_API
size_t nad_deque_bytes(const nad_Deque *self);

[[nodiscard]] NAD_API
nad_Al *nad_deque_al(const nad_Deque *self);

/* ========== access ========== */

[[nodiscard]] NAD_API
const void *nad_deque_first(const nad_Deque *self);

[[nodiscard]] NAD_API
void *nad_deque_first_mut(nad_Deque *self);

[[nodiscard]] NAD_API
const void *nad_deque_last(const nad_Deque *self);

[[nodiscard]] NAD_API
void *nad_deque_last_mut(nad_Deque *self);

/// 'idx' counts from the front, so 0 is always the front elem wherever the ring starts
[[nodiscard]] NAD_API
const void *nad_deque_get(const nad_Deque *self, size_t idx);

[[nodiscard]] NAD_API
void *nad_deque_get_mut(nad_Deque *self, size_t idx);

NAD_API
void nad_deque_set(nad_Deque *self, size_t idx, const void *val);

/* ========== mods ========== */

/// 'val' must not point into this deque's own buffer: a push that grows moves the elems
/// out from under it
[[nodiscard]] NAD_API
nad_Status nad_deque_push_front(nad_Deque *self, const void *val);

/// 'val' must not point into this deque's own buffer, as in push_front
[[nodiscard]] NAD_API
nad_Status nad_deque_push_back(nad_Deque *self, const void *val);

NAD_API
void nad_deque_pop_front(nad_Deque *self);

NAD_API
void nad_deque_pop_back(nad_Deque *self);

/// 'idx == len' is legal and means push_back. Shifts whichever side is shorter, so this
/// is O(n) with half the constant of a vec — but it is still O(n), and the two ends are
/// what the type is for. 'val' must not point into this deque's own buffer, as in
/// push_front
[[nodiscard]] NAD_API
nad_Status nad_deque_insert(nad_Deque *self, size_t idx, const void *val);

NAD_API
void nad_deque_remove(nad_Deque *self, size_t idx);

NAD_API
void nad_deque_clear(nad_Deque *self);

[[nodiscard]] NAD_API
nad_Status nad_deque_reserve(nad_Deque *self, size_t new_cap);

[[nodiscard]] NAD_API
nad_Status nad_deque_shrink_to_fit(nad_Deque *self);

/// grows at the BACK with zeroed elems and shrinks from the back — the front stays put,
/// so a resize never renumbers what was already there
[[nodiscard]] NAD_API
nad_Status nad_deque_resize(nad_Deque *self, size_t new_len);

/// on one allocator the buffers are handed over and the capacity travels with them;
/// on two the bytes are moved and each side is left sized to its new content
[[nodiscard]] NAD_API
nad_Status nad_deque_swap(nad_Deque *self, nad_Deque *other);

NAD_API
void nad_deque_swap_elems(nad_Deque *self, size_t i, size_t j);

/* ========== print ========== */

NAD_API
void nad_deque_fprint(const nad_Deque *self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_deque_print(const nad_Deque *self, nad_FPrint fprint);

/* ========== macros ========== */

#define NAD_DEQUE_NEW(T, al, out) \
    nad_deque_new(sizeof(T), (al), (out))

#define NAD_DEQUE_NEW_LEN(T, len, al, out) \
    nad_deque_new_len((len), sizeof(T), (al), (out))

#define NAD_DEQUE_NEW_CAP(T, cap, al, out) \
    nad_deque_new_cap((cap), sizeof(T), (al), (out))

#define NAD_DEQUE_FROM_DATA(T, data, len, al, out) \
    nad_deque_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

#define NAD_DEQUE_OF(T, al, out, ...)                   \
    nad_deque_from_data(                                \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

#define NAD_DEQUE_FIRST_AS(T, self) \
    ((const T *) nad_deque_first((self)))

#define NAD_DEQUE_FIRST_MUT_AS(T, self) \
    ((T *) nad_deque_first_mut((self)))

#define NAD_DEQUE_LAST_AS(T, self) \
    ((const T *) nad_deque_last((self)))

#define NAD_DEQUE_LAST_MUT_AS(T, self) \
    ((T *) nad_deque_last_mut((self)))

#define NAD_DEQUE_GET_AS(T, self, idx) \
    ((const T *) nad_deque_get((self), (idx)))

#define NAD_DEQUE_GET_MUT_AS(T, self, idx) \
    ((T *) nad_deque_get_mut((self), (idx)))

#define NAD_DEQUE_SET(T, self, idx, val) \
    nad_deque_set((self), (idx), &(T){ (val) })

#define NAD_DEQUE_PUSH_FRONT(T, self, val) \
    nad_deque_push_front((self), &(T){ (val) })

#define NAD_DEQUE_PUSH_BACK(T, self, val) \
    nad_deque_push_back((self), &(T){ (val) })

#define NAD_DEQUE_INSERT(T, self, idx, val) \
    nad_deque_insert((self), (idx), &(T){ (val) })
