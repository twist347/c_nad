#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// owning first in first out queue: elems join at the back and leave from the front,
/// both in O(1) amortized. A ds/deque holds them — it is the container here whose two
/// ends are equally cheap, which is exactly the pair a queue needs. Over a vec the same
/// type would be O(n) per pop, which is not a queue at all.
///
/// What this adds over the deque it wraps is a NARROWER interface, not an invariant over
/// the elems: no get by index, no insert, no remove, no push_front, no pop_back. Holding
/// a nad_Queue is a statement about how the data is used, and the compiler keeps it.
///
/// The order of the elems IS the queue's meaning — it is the order they arrived in. So
/// the bridge to algo runs one way only: nad_queue_copy_to_span hands out a copy to read
/// (search it, count it, fold it) and there is no copy_from_span to write one back.
/// Sorting a queue would not be an operation on a queue. For the same reason there is no
/// to_span: the elems may wrap the deque's ring, so there is no run of bytes to view.
///
/// The pointers are not stable either: growing moves every elem, as in the deque and the
/// vec. That is what ds/list is for.
typedef struct nad_Queue nad_Queue;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Status nad_queue_new(size_t elem_size, nad_Al *al, nad_Queue **out);

[[nodiscard]] NAD_API
nad_Status nad_queue_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Queue **out);

/// the elems join the queue in the order they are given, so 'data[0]' is the one served
/// first
[[nodiscard]] NAD_API
nad_Status nad_queue_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Queue **out);

[[nodiscard]] NAD_API
nad_Status nad_queue_from_span(nad_Span s, nad_Al *al, nad_Queue **out);

NAD_API
void nad_queue_drop(nad_Queue *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API
nad_Status nad_queue_copy(const nad_Queue *self, nad_Queue **out);

[[nodiscard]] NAD_API
nad_Status nad_queue_copy_assign(const nad_Queue *self, nad_Queue *other);

/// writes every elem into 'dst' front to back. 'dst' must be exactly as long as the
/// queue, the same rule nad_span_copy holds callers to. This is the whole bridge to algo,
/// and it is read only by design — see the note on the type
NAD_API
void nad_queue_copy_to_span(const nad_Queue *self, nad_SpanMut dst);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_queue_len(const nad_Queue *self);

[[nodiscard]] NAD_API
size_t nad_queue_cap(const nad_Queue *self);

[[nodiscard]] NAD_API
size_t nad_queue_elem_size(const nad_Queue *self);

[[nodiscard]] NAD_API
nad_Al *nad_queue_al(const nad_Queue *self);

/* ========== access ========== */

/// the elem to be served next; the queue must not be empty. Named front rather than
/// first because a queue names roles, not places in a sequence — the deque underneath
/// does the latter
[[nodiscard]] NAD_API
const void *nad_queue_front(const nad_Queue *self);

/// unlike a pqueue there is no order over the elems to break, so a writable end is safe:
/// what makes a queue a queue is where elems enter and leave, not what they hold
[[nodiscard]] NAD_API
void *nad_queue_front_mut(nad_Queue *self);

/// the elem that arrived most recently; the queue must not be empty
[[nodiscard]] NAD_API
const void *nad_queue_back(const nad_Queue *self);

[[nodiscard]] NAD_API
void *nad_queue_back_mut(nad_Queue *self);

/* ========== mods ========== */

/// joins the back of the queue. 'val' must not point into the queue's own elems: a push
/// that grows moves them out from under it
[[nodiscard]] NAD_API
nad_Status nad_queue_push(nad_Queue *self, const void *val);

/// drops the front elem; the queue must not be empty. Read it with front first — a pop
/// that returned the elem would have to copy it somewhere, and there is nowhere to put it
NAD_API
void nad_queue_pop(nad_Queue *self);

NAD_API
void nad_queue_clear(nad_Queue *self);

[[nodiscard]] NAD_API
nad_Status nad_queue_reserve(nad_Queue *self, size_t new_cap);

[[nodiscard]] NAD_API
nad_Status nad_queue_shrink_to_fit(nad_Queue *self);

/// on one allocator the buffers are handed over and the capacity travels with them;
/// on two the bytes are moved and each side is left sized to its new content
[[nodiscard]] NAD_API
nad_Status nad_queue_swap(nad_Queue *self, nad_Queue *other);

/* ========== print ========== */

NAD_API
void nad_queue_fprint(const nad_Queue *self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_queue_print(const nad_Queue *self, nad_FPrint fprint);

/* ========== macros ========== */

#define NAD_QUEUE_NEW(T, al, out) \
    nad_queue_new(sizeof(T), (al), (out))

#define NAD_QUEUE_NEW_CAP(T, cap, al, out) \
    nad_queue_new_cap((cap), sizeof(T), (al), (out))

#define NAD_QUEUE_FROM_DATA(T, data, len, al, out) \
    nad_queue_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

#define NAD_QUEUE_OF(T, al, out, ...)                   \
    nad_queue_from_data(                                \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

#define NAD_QUEUE_FRONT_AS(T, self) \
    ((const T *) nad_queue_front((self)))

#define NAD_QUEUE_FRONT_MUT_AS(T, self) \
    ((T *) nad_queue_front_mut((self)))

#define NAD_QUEUE_BACK_AS(T, self) \
    ((const T *) nad_queue_back((self)))

#define NAD_QUEUE_BACK_MUT_AS(T, self) \
    ((T *) nad_queue_back_mut((self)))

#define NAD_QUEUE_PUSH(T, self, val) \
    nad_queue_push((self), &(T){ (val) })
