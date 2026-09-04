#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"
#include "nad/ds/deque.h"

#include <stddef.h>

/// @file

/// @defgroup ds_queue ds/queue
/// @ingroup ds
/// @brief nad_Queue — an owning first in first out queue
///
/// Elems join at the back and leave from the front, both in O(1) amortized. A ds/deque
/// holds them: it is the container whose two ends are equally cheap, where over a vec the
/// same type would be O(n) per pop.
///
/// What this adds over the deque it wraps is a NARROWER interface, not an invariant over
/// the elems: no get by index, no insert, no remove, no push_front, no pop_back.
///
/// The order of the elems IS the queue's meaning, so the bridge to algo runs one way
/// only: nad_queue_copy_to_span hands out a copy to read, and there is no copy_from_span
/// to write one back. There is no to_span either — the elems may wrap the ring, so there
/// is no run of bytes to view.
///
/// The pointers are not stable: growing moves every elem. That is what ds/list is for.
///
/// @par Example
/// @snippet ds/example_queue.c build
/// @snippet ds/example_queue.c fifo
/// @snippet ds/example_queue.c read
/// @snippet ds/example_queue.c into
/// @{

/// Owning first in first out queue.
/// An opaque handle: it comes from a constructor and goes back to nad_queue_drop
typedef struct nad_Queue nad_Queue;

/// @name lifetime
/// @{

/// an empty queue that owns no block yet
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator, kept for everything after
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the deque cannot be allocated
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Status nad_queue_new(size_t elem_size, nad_Al *al, nad_Queue **out);

/// an empty queue with room for 'cap' elems before the first growth
/// @param cap how many elems to make room for
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot be allocated, or
///         cap * elem_size overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_queue_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Queue **out);

/// a queue holding a copy of 'len' elems read from 'data', joined in the order given
/// @param data the elems to copy in; 'data[0]' is the one served first. May be null only
///             when len is 0
/// @param len how many elems to read
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_queue_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Queue **out);

/// a queue holding a copy of what 's' views, taking its elem_size
/// @param s the view to copy; its first elem is served first
/// @param al the allocator
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_queue_from_span(nad_Span s, nad_Al *al, nad_Queue **out);

/// releases the elems and the queue through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object
/// @bigo{1}
NAD_API
void nad_queue_drop(nad_Queue *self);

/// hands the elems over to the deque that held them and releases the queue around it
/// @param self consumed: its header goes back to the allocator, and the handle must not
///             be used again. Null is not allowed — there would be nothing to hand back
/// @return the deque, holding the elems front to back with the capacity and the allocator
///         they already had. Nothing is copied, so nothing can fail
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Deque *nad_queue_into_deque(nad_Queue *self);

/// @}

/// @name copy
/// @{

/// a new queue with the same elems in the same order, on the same allocator
/// @param self the queue to copy
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_queue_copy(const nad_Queue *self, nad_Queue **out);

/// a new queue with the same elems in the same order, on 'al'
/// @param self the queue to copy
/// @param al where the copy lives; nad_queue_copy is this one with the allocator of 'self'
/// @param[out] out the new queue, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_queue_copy_with(const nad_Queue *self, nad_Al *al, nad_Queue **out);

/// overwrites the elems of 'other' with those of 'self', growing its block when it must
/// @param self the queue to copy from
/// @param[in,out] other must have the same elem_size; keeps its own allocator, and
///                      'self' == 'other' is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow, leaving 'other' as it was
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_queue_copy_assign(const nad_Queue *self, nad_Queue *other);

/// writes every elem into 'dst' front to back. The whole bridge to algo, and read only by
/// design — see the note on the type
/// @param self the queue
/// @param dst must have the same elem_size and be exactly as long as the queue
/// @bigo{n}
NAD_API
void nad_queue_copy_to_span(const nad_Queue *self, nad_SpanMut dst);

/// @}

/// @name compare
/// @{

/// whether the two hold the same elems in queue order, byte for byte
/// @param a one queue
/// @param b must have the same elem_size — a mismatch there is a programmer error, not a
///          false; a differing length is just false
/// @return whether the lengths match and the bytes do
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_queue_eq(const nad_Queue *a, const nad_Queue *b);

/// whether the two hold equal elems under 'eq'
/// @param a one queue
/// @param b must have the same elem_size as 'a'
/// @param eq asked of every pair until one says no
/// @return whether the lengths match and every pair does
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_queue_eq_by(const nad_Queue *a, const nad_Queue *b, nad_Eq eq);

/// @}

/// @name info
/// @{

/// how many elems are waiting
/// @param self the queue
/// @return the length, never above the capacity
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_queue_len(const nad_Queue *self);

/// how many elems fit before the block must grow
/// @param self the queue
/// @return the capacity
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_queue_cap(const nad_Queue *self);

/// the size of one elem, as named at construction
/// @param self the queue
/// @return elem_size, which never moves
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_queue_elem_size(const nad_Queue *self);

/// the allocator the queue was built with
/// @param self the queue
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_queue_al(const nad_Queue *self);

/// @}

/// @name access
/// @{

/// the elem to be served next. Named front rather than first because a queue names roles,
/// not places in a sequence
/// @param self asserts the queue is not empty
/// @return a pointer into the block, good until the next push
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_queue_front(const nad_Queue *self);

/// the front elem, to write through — what makes a queue a queue is where elems enter and
/// leave, not what they hold
/// @copydetails nad_queue_front
[[nodiscard]] NAD_API
void *nad_queue_front_mut(nad_Queue *self);

/// the elem that arrived most recently
/// @param self asserts the queue is not empty
/// @return a pointer into the block, good until the next push
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_queue_back(const nad_Queue *self);

/// the back elem, to write through
/// @copydetails nad_queue_back
[[nodiscard]] NAD_API
void *nad_queue_back_mut(nad_Queue *self);

/// @}

/// @name mods
/// @{

/// joins a copy of 'val' to the back
/// @param self the queue
/// @param val must not point into the queue's own elems: a push that grows moves them out
///            from under it
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{1} amortized
[[nodiscard]] NAD_API
nad_Status nad_queue_push(nad_Queue *self, const void *val);

/// drops the front elem, keeping the capacity
/// @param self asserts the queue is not empty. Read the elem with nad_queue_front first —
///             a pop that returned it would have nowhere to put it
/// @bigo{1}
NAD_API
void nad_queue_pop(nad_Queue *self);

/// drops every elem, keeping the block
/// @param self the queue
/// @bigo{1}
NAD_API
void nad_queue_clear(nad_Queue *self);

/// makes room for 'new_cap' elems
/// @param self the queue
/// @param new_cap a capacity at or below the one it has is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow, or new_cap * elem_size
///         overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_queue_reserve(nad_Queue *self, size_t new_cap);

/// gives back the room above the length
/// @param self a length of 0 releases the block outright
/// @retval NAD_STATUS_OK on success, and when there was nothing to give back
/// @retval NAD_STATUS_ERR_NO_MEM when the allocator refuses the smaller block, leaving
///         the queue as it was
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_queue_shrink_to_fit(nad_Queue *self);

/// exchanges the contents of the two
/// @param self one queue
/// @param other must have the same elem_size
/// @retval NAD_STATUS_OK on success; on one allocator the blocks are handed over and the
///         capacity travels with them, on two the bytes are moved and each side is left
///         sized to its new content
/// @retval NAD_STATUS_ERR_NO_MEM when the two sit on different allocators and a block
///         cannot be taken, leaving both as they were
/// @bigo{1} on one allocator, n on two
[[nodiscard]] NAD_API
nad_Status nad_queue_swap(nad_Queue *self, nad_Queue *other);

/// @}

/// @name print
/// @{

/// writes the elems to a stream front to back as [a, b, c], followed by a newline
/// @param self the queue
/// @param stream where to write
/// @param fprint the printer, called once per elem
/// @bigo{n}
NAD_API
void nad_queue_fprint(const nad_Queue *self, FILE *stream, nad_FPrint fprint);

/// nad_queue_fprint to stdout
/// @param self the queue
/// @param fprint the printer, called once per elem
/// @bigo{n}
NAD_API
void nad_queue_print(const nad_Queue *self, nad_FPrint fprint);

/// @}

/// @name macros
/// @{

/// nad_queue_new with sizeof(T) for the elem size
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new queue is written
/// @bigo{1}
#define NAD_QUEUE_NEW(T, al, out) \
    nad_queue_new(sizeof(T), (al), (out))

/// nad_queue_new_cap with sizeof(T)
/// @param T the elem type
/// @param cap how many elems to make room for
/// @param al the allocator
/// @param[out] out where the new queue is written
/// @bigo{n}
#define NAD_QUEUE_NEW_CAP(T, cap, al, out) \
    nad_queue_new_cap((cap), sizeof(T), (al), (out))

/// nad_queue_from_data with sizeof(T)
/// @param T the elem type
/// @param data the elems to copy in, made to typecheck as a const T *
/// @param len how many elems to read from 'data'
/// @param al the allocator
/// @param[out] out where the new queue is written
/// @bigo{n}
#define NAD_QUEUE_FROM_DATA(T, data, len, al, out) \
    nad_queue_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

/// a new queue from the elems written out: NAD_QUEUE_OF(int32_t, al, &q, 5, 3, 1)
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new queue is written
/// @param ... the elems, front to back, as a T initializer list
/// @bigo{n}
#define NAD_QUEUE_OF(T, al, out, ...)                   \
    nad_queue_from_data(                                \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

/// nad_queue_front as a const T *
/// @param T the elem type
/// @param self the queue
/// @bigo{1}
#define NAD_QUEUE_FRONT_AS(T, self) \
    ((const T *) nad_queue_front((self)))

/// nad_queue_front_mut as a T *
/// @copydetails NAD_QUEUE_FRONT_AS
#define NAD_QUEUE_FRONT_MUT_AS(T, self) \
    ((T *) nad_queue_front_mut((self)))

/// nad_queue_back as a const T *
/// @copydetails NAD_QUEUE_FRONT_AS
#define NAD_QUEUE_BACK_AS(T, self) \
    ((const T *) nad_queue_back((self)))

/// nad_queue_back_mut as a T *
/// @copydetails NAD_QUEUE_FRONT_AS
#define NAD_QUEUE_BACK_MUT_AS(T, self) \
    ((T *) nad_queue_back_mut((self)))

/// nad_queue_push from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the queue
/// @param val the value to copy in
/// @bigo{1} amortized
#define NAD_QUEUE_PUSH(T, self, val) \
    nad_queue_push((self), &(T){ (val) })

/// @}

/// @}
