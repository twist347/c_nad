#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// owning priority queue: a growable buffer kept under the heap discipline of algo/heap,
/// so the greatest elem by 'cmp' is always the one at the front. A min-queue is this same
/// type built with a descending comparator (nad_cmp_desc_i32 and friends) — there is
/// deliberately no second type.
///
/// The comparator is a property of the queue, not of a call: it is fixed at construction
/// and travels with the elems through copy and swap, since a heap means nothing without
/// the order it was built under.
///
/// Nothing here hands out a mutable elem — no top_mut, no get, no to_span_mut. A write
/// through such a pointer would break the heap invariant with no way for the queue to
/// notice, so the only way in is push and the only way out is pop.
typedef struct nad_PQueue nad_PQueue;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Status nad_pqueue_new(size_t elem_size, nad_Cmp cmp, nad_Al *al, nad_PQueue **out);

[[nodiscard]] NAD_API
nad_Status nad_pqueue_new_cap(size_t cap, size_t elem_size, nad_Cmp cmp, nad_Al *al, nad_PQueue **out);

/// takes the elems in any order and heapifies them in O(len) — cheaper than 'len' pushes,
/// which cost O(len log len)
[[nodiscard]] NAD_API
nad_Status nad_pqueue_from_data(const void *data, size_t len, size_t elem_size, nad_Cmp cmp, nad_Al *al, nad_PQueue **out);

[[nodiscard]] NAD_API
nad_Status nad_pqueue_from_span(nad_Span s, nad_Cmp cmp, nad_Al *al, nad_PQueue **out);

NAD_API
void nad_pqueue_drop(nad_PQueue *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API
nad_Status nad_pqueue_copy(const nad_PQueue *self, nad_PQueue **out);

/// 'other' receives the comparator along with the elems, overwriting its own
[[nodiscard]] NAD_API
nad_Status nad_pqueue_copy_assign(const nad_PQueue *self, nad_PQueue *other);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_pqueue_len(const nad_PQueue *self);

[[nodiscard]] NAD_API
size_t nad_pqueue_cap(const nad_PQueue *self);

[[nodiscard]] NAD_API
size_t nad_pqueue_elem_size(const nad_PQueue *self);

[[nodiscard]] NAD_API
nad_Al *nad_pqueue_al(const nad_PQueue *self);

[[nodiscard]] NAD_API
nad_Cmp nad_pqueue_cmp(const nad_PQueue *self);

/* ========== access ========== */

/// the greatest elem; the queue must not be empty
[[nodiscard]] NAD_API
const void *nad_pqueue_top(const nad_PQueue *self);

/* ========== mods ========== */

[[nodiscard]] NAD_API
nad_Status nad_pqueue_push(nad_PQueue *self, const void *val);

/// drops the greatest elem; the queue must not be empty. Read it with top first — a pop
/// that returned the elem would have to copy it somewhere, and there is nowhere to put it
NAD_API
void nad_pqueue_pop(nad_PQueue *self);

NAD_API
void nad_pqueue_clear(nad_PQueue *self);

[[nodiscard]] NAD_API
nad_Status nad_pqueue_reserve(nad_PQueue *self, size_t new_cap);

[[nodiscard]] NAD_API
nad_Status nad_pqueue_shrink_to_fit(nad_PQueue *self);

/// the comparator changes sides with the elems, so two queues under different orders
/// stay valid queues afterwards
[[nodiscard]] NAD_API
nad_Status nad_pqueue_swap(nad_PQueue *self, nad_PQueue *other);

/* ========== to span ========== */

/// the elems in heap order, which is not sorted order: only the first is in its final
/// place. Read only, because the order is the queue's to keep
[[nodiscard]] NAD_API
nad_Span nad_pqueue_to_span(const nad_PQueue *self);

/* ========== print ========== */

NAD_API
void nad_pqueue_fprint(const nad_PQueue *self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_pqueue_print(const nad_PQueue *self, nad_FPrint fprint);

/* ========== macros ========== */

#define NAD_PQUEUE_NEW(T, cmp, al, out) \
    nad_pqueue_new(sizeof(T), (cmp), (al), (out))

#define NAD_PQUEUE_NEW_CAP(T, cap, cmp, al, out) \
    nad_pqueue_new_cap((cap), sizeof(T), (cmp), (al), (out))

#define NAD_PQUEUE_FROM_DATA(T, data, len, cmp, al, out) \
    nad_pqueue_from_data((const T *){ (data) }, (len), sizeof(T), (cmp), (al), (out))

#define NAD_PQUEUE_OF(T, cmp, al, out, ...)             \
    nad_pqueue_from_data(                               \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (cmp), (al), (out))

#define NAD_PQUEUE_TOP_AS(T, self) \
    ((const T *) nad_pqueue_top((self)))

#define NAD_PQUEUE_PUSH(T, self, val) \
    nad_pqueue_push((self), &(T){ (val) })
