#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"
#include "nad/ds/vec.h"

#include <stddef.h>

/// @file

/// @defgroup ds_pqueue ds/pqueue
/// @ingroup ds
/// @brief nad_PQueue — an owning queue that serves the greatest elem first
///
/// A growable buffer kept under the heap discipline of algo/heap, so the greatest elem by
/// 'cmp' is always the one at the front. A min-queue is this same type built with a
/// descending comparator (nad_cmp_desc_i32 and friends) — there is deliberately no second
/// type.
///
/// The comparator is fixed at construction and travels with the elems through copy and
/// swap, since a heap means nothing without the order it was built under.
///
/// There is deliberately no nad_pqueue_eq: equal contents do not make equal heaps, since
/// the same elems pushed in another order lie in another arrangement. An honest answer
/// would have to sort a copy. Two queues are compared by draining them.
///
/// Nothing here hands out a mutable elem — no top_mut, no get, no to_span_mut: a write
/// through one would break the heap invariant with no way for the queue to notice.
///
/// @par Example
/// @snippet ds/example_pqueue.c build
/// @snippet ds/example_pqueue.c serve
/// @snippet ds/example_pqueue.c order
/// @snippet ds/example_pqueue.c into
/// @{

/// Owning queue that serves the greatest elem first.
/// An opaque handle: it comes from a constructor and goes back to nad_pqueue_drop
typedef struct nad_PQueue nad_PQueue;

/// @name lifetime
/// @{

/// an empty queue that owns no block yet
/// @param elem_size the size of one elem, asserted greater than 0
/// @param cmp the order to serve in, fixed for the life of the queue
/// @param al the allocator, kept for everything after
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the vec cannot be allocated
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Status nad_pqueue_new(size_t elem_size, nad_Cmp cmp, nad_Al *al, nad_PQueue **out);

/// an empty queue with room for 'cap' elems before the first growth
/// @param cap how many elems to make room for
/// @param elem_size the size of one elem, asserted greater than 0
/// @param cmp the order to serve in
/// @param al the allocator
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot be allocated, or
///         cap * elem_size overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_pqueue_new_cap(size_t cap, size_t elem_size, nad_Cmp cmp, nad_Al *al, nad_PQueue **out);

/// a queue over a copy of 'len' elems read from 'data', in any order
/// @param data the elems to copy in; may be null only when len is 0
/// @param len how many elems to read
/// @param elem_size the size of one elem, asserted greater than 0
/// @param cmp the order to serve in
/// @param al the allocator
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n} — heapifying in one pass is cheaper than 'len' pushes, which cost O(n log n)
[[nodiscard]] NAD_API
nad_Status nad_pqueue_from_data(const void *data, size_t len, size_t elem_size, nad_Cmp cmp, nad_Al *al,
                                nad_PQueue **out);

/// a queue over a copy of what 's' views, taking its elem_size
/// @param s the view to copy, in any order
/// @param cmp the order to serve in
/// @param al the allocator
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_pqueue_from_span(nad_Span s, nad_Cmp cmp, nad_Al *al, nad_PQueue **out);

/// releases the elems and the queue through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object
/// @bigo{1}
NAD_API
void nad_pqueue_drop(nad_PQueue *self);

/// hands the elems over to the vec that held them and releases the queue around it
/// @param self consumed: its header goes back to the allocator, and the handle must not
///             be used again. Null is not allowed — there would be nothing to hand back
/// @return the vec, holding the elems in HEAP order rather than sorted, with the capacity
///         and the allocator they already had. The comparator does not travel with them,
///         being the queue's rather than the elems'; nad_span_sort_heap over
///         nad_vec_to_span_mut finishes the sort in place. Nothing is copied, so nothing
///         can fail
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Vec *nad_pqueue_into_vec(nad_PQueue *self);

/// @}

/// @name copy
/// @{

/// a new queue with the same elems and comparator, on the same allocator
/// @param self the queue to copy
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n} — the arrangement is copied as it is, so nothing is reheapified
[[nodiscard]] NAD_API
nad_Status nad_pqueue_copy(const nad_PQueue *self, nad_PQueue **out);

/// overwrites the elems of 'other' with those of 'self', growing its block when it must
/// @param self the queue to copy from
/// @param[in,out] other must have the same elem_size; receives the comparator along with
///                      the elems, overwriting its own, and keeps its own allocator.
///                      'self' == 'other' is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow, leaving 'other' as it was
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_pqueue_copy_assign(const nad_PQueue *self, nad_PQueue *other);

/// @}

/// @name info
/// @{

/// how many elems are waiting
/// @param self the queue
/// @return the length, never above the capacity
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_pqueue_len(const nad_PQueue *self);

/// how many elems fit before the block must grow
/// @param self the queue
/// @return the capacity
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_pqueue_cap(const nad_PQueue *self);

/// the size of one elem, as named at construction
/// @param self the queue
/// @return elem_size, which never moves
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_pqueue_elem_size(const nad_PQueue *self);

/// the allocator the queue was built with
/// @param self the queue
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_pqueue_al(const nad_PQueue *self);

/// the order the queue serves in
/// @param self the queue
/// @return the comparator, as given at construction; it moves only through copy_assign
///         and swap
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Cmp nad_pqueue_cmp(const nad_PQueue *self);

/// @}

/// @name access
/// @{

/// the greatest elem by 'cmp', the one the next pop drops
/// @param self asserts the queue is not empty
/// @return a read-only pointer into the block, good until the next push or pop. There is
///         no mutable form: a write through one would break the heap invariant
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_pqueue_top(const nad_PQueue *self);

/// @}

/// @name mods
/// @{

/// puts a copy of 'val' in, sifting it up to its place
/// @param self the queue
/// @param val must not point into the queue's own elems: a push that grows moves them out
///            from under it
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{log n} — plus the amortized cost of growing
[[nodiscard]] NAD_API
nad_Status nad_pqueue_push(nad_PQueue *self, const void *val);

/// drops the greatest elem and sifts the next one up
/// @param self asserts the queue is not empty. Read the elem with nad_pqueue_top first —
///             a pop that returned it would have nowhere to put it
/// @bigo{log n}
NAD_API
void nad_pqueue_pop(nad_PQueue *self);

/// drops every elem, keeping the block
/// @param self the queue
/// @bigo{1}
NAD_API
void nad_pqueue_clear(nad_PQueue *self);

/// makes room for 'new_cap' elems
/// @param self the queue
/// @param new_cap a capacity at or below the one it has is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow, or new_cap * elem_size
///         overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_pqueue_reserve(nad_PQueue *self, size_t new_cap);

/// gives back the room above the length
/// @param self a length of 0 releases the block outright
/// @retval NAD_STATUS_OK on success, and when there was nothing to give back
/// @retval NAD_STATUS_ERR_NO_MEM when the allocator refuses the smaller block, leaving
///         the queue as it was
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_pqueue_shrink_to_fit(nad_PQueue *self);

/// exchanges the contents of the two, comparators included
/// @param self one queue
/// @param other must have the same elem_size; the comparator changes sides with the
///              elems, so two queues under different orders stay valid queues afterwards
/// @retval NAD_STATUS_OK on success; on one allocator the blocks are handed over and the
///         capacity travels with them, on two the bytes are moved and each side is left
///         sized to its new content
/// @retval NAD_STATUS_ERR_NO_MEM when the two sit on different allocators and a block
///         cannot be taken, leaving both as they were
/// @bigo{1} on one allocator, n on two
[[nodiscard]] NAD_API
nad_Status nad_pqueue_swap(nad_PQueue *self, nad_PQueue *other);

/// @}

/// @name to span
/// @{

/// the elems in heap order, which is not sorted order: only the first is in its final
/// place
/// @param self the queue
/// @return a read-only view, good until the next push or pop. Read only because the
///         arrangement is the queue's to keep
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Span nad_pqueue_to_span(const nad_PQueue *self);

/// @}

/// @name print
/// @{

/// writes the elems to a stream in heap order as [a, b, c], followed by a newline
/// @param self the queue
/// @param stream where to write
/// @param fprint the printer, called once per elem
/// @bigo{n}
NAD_API
void nad_pqueue_fprint(const nad_PQueue *self, FILE *stream, nad_FPrint fprint);

/// nad_pqueue_fprint to stdout
/// @param self the queue
/// @param fprint the printer, called once per elem
/// @bigo{n}
NAD_API
void nad_pqueue_print(const nad_PQueue *self, nad_FPrint fprint);

/// @}

/// @name macros
/// @{

/// nad_pqueue_new with sizeof(T) for the elem size
/// @param T the elem type
/// @param cmp the order to serve in
/// @param al the allocator
/// @param[out] out where the new queue is written
/// @bigo{1}
#define NAD_PQUEUE_NEW(T, cmp, al, out) \
    nad_pqueue_new(sizeof(T), (cmp), (al), (out))

/// nad_pqueue_new_cap with sizeof(T)
/// @param T the elem type
/// @param cap how many elems to make room for
/// @param cmp the order to serve in
/// @param al the allocator
/// @param[out] out where the new queue is written
/// @bigo{n}
#define NAD_PQUEUE_NEW_CAP(T, cap, cmp, al, out) \
    nad_pqueue_new_cap((cap), sizeof(T), (cmp), (al), (out))

/// nad_pqueue_from_data with sizeof(T)
/// @param T the elem type
/// @param data the elems to copy in, made to typecheck as a const T *
/// @param len how many elems to read from 'data'
/// @param cmp the order to serve in
/// @param al the allocator
/// @param[out] out where the new queue is written
/// @bigo{n}
#define NAD_PQUEUE_FROM_DATA(T, data, len, cmp, al, out) \
    nad_pqueue_from_data((const T *){ (data) }, (len), sizeof(T), (cmp), (al), (out))

/// a new queue from the elems written out: NAD_PQUEUE_OF(int32_t, nad_cmp_i32, al, &q, 5, 1)
/// @param T the elem type
/// @param cmp the order to serve in
/// @param al the allocator
/// @param[out] out where the new queue is written
/// @param ... the elems, in any order, as a T initializer list
/// @bigo{n}
#define NAD_PQUEUE_OF(T, cmp, al, out, ...)             \
    nad_pqueue_from_data(                               \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (cmp), (al), (out))

/// nad_pqueue_top as a const T *
/// @param T the elem type
/// @param self the queue
/// @bigo{1}
#define NAD_PQUEUE_TOP_AS(T, self) \
    ((const T *) nad_pqueue_top((self)))

/// nad_pqueue_push from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the queue
/// @param val the value to copy in
/// @bigo{log n}
#define NAD_PQUEUE_PUSH(T, self, val) \
    nad_pqueue_push((self), &(T){ (val) })

/// @}

/// @}
