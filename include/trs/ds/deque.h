#pragma once

#include "trs/alloc/alloc.h"
#include "trs/core/cmp.h"
#include "trs/core/export.h"
#include "trs/core/print.h"
#include "trs/core/span.h"
#include "trs/core/status.h"

#include <stddef.h>

/// @file

/// @defgroup ds_deque ds/deque
/// @ingroup ds
/// @brief trs_Deque — an owning queue with two cheap ends
///
/// A growable ring buffer over one block: both ends cost O(1) amortized and get by index
/// stays O(1), since the elem for 'idx' sits at '(head + idx) % cap'.
///
/// The contents may therefore WRAP: they are one run of elems in ring order, not one run
/// of bytes. Two things follow, both absent on purpose.
///
/// There is no trs_deque_data: there is no single block to point at. There is no
/// trs_deque_to_span either, and no linearize to earn one — making the ring contiguous
/// would be a mutation the next push_front undoes. The bridge to algo is the pair
/// trs_deque_copy_to_span / trs_deque_copy_from_span, which copies instead of rearranging.
///
/// The pointers are not stable: growing moves every elem. That is what ds/list is for.
///
/// An index out of range asserts; the ops that return a trs_Status are the ones that
/// allocate.
///
/// @par Example
/// @snippet ds/example_deque.c build
/// @snippet ds/example_deque.c ends
/// @snippet ds/example_deque.c index
/// @snippet ds/example_deque.c algo
/// @{

/// Owning queue with two cheap ends.
/// An opaque handle: it comes from a constructor and goes back to trs_deque_drop
typedef struct trs_Deque trs_Deque;

/// @name lifetime
/// @{

/// an empty deque that owns no block yet
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator, kept for everything after
/// @param[out] out the new deque, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header cannot be allocated
/// @bigo{1}
[[nodiscard]] TRS_API
trs_Status trs_deque_new(size_t elem_size, trs_Al *al, trs_Deque **out);

/// a deque of 'len' zeroed elems, ready to be written through get_mut or set
/// @param len how many elems
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new deque, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_new_len(size_t len, size_t elem_size, trs_Al *al, trs_Deque **out);

/// an empty deque with room for 'cap' elems before the first growth
/// @param cap how many elems to make room for
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new deque, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         cap * elem_size overflows
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_new_cap(size_t cap, size_t elem_size, trs_Al *al, trs_Deque **out);

/// a deque holding a copy of 'len' elems read from 'data', front to back
/// @param data the elems to copy in; may be null only when len is 0
/// @param len how many elems to read
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new deque, written only on success; its ring starts out unwrapped
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_from_data(const void *data, size_t len, size_t elem_size, trs_Al *al, trs_Deque **out);

/// a deque holding a copy of what 's' views, taking its elem_size
/// @param s the view to copy
/// @param al the allocator
/// @param[out] out the new deque, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_from_span(trs_Span s, trs_Al *al, trs_Deque **out);

/// releases the block and the deque through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object
/// @bigo{1}
TRS_API
void trs_deque_drop(trs_Deque *self);

/// @}

/// @name copy
/// @{

/// a new deque with the same elems in the same order, on the same allocator
/// @param self the deque to copy
/// @param[out] out the new deque, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_copy(const trs_Deque *self, trs_Deque **out);

/// a new deque with the same elems in the same order, on 'al'
/// @param self the deque to copy
/// @param al where the copy lives; trs_deque_copy is this one with the allocator of 'self'
/// @param[out] out the new deque, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_copy_with(const trs_Deque *self, trs_Al *al, trs_Deque **out);

/// overwrites the elems of 'other' with those of 'self', growing its block when it must
/// @param self the deque to copy from
/// @param[in,out] other must have the same elem_size; keeps its own allocator, and
///                      'self' == 'other' is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow, leaving 'other' as it was
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_copy_assign(const trs_Deque *self, trs_Deque *other);

/// moves the elems of 'self' into 'other', leaving 'self' empty
/// @param[in,out] self the deque to move from; emptied on success and still usable, on
///                     its own allocator
/// @param[in,out] other must have the same elem_size; releases what it held and keeps its own allocator. 'self' == 'other' is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the two sit on different allocators and the block cannot be taken,
///         leaving both as they were
/// @bigo{1} on one allocator, n on two — the block belongs to the allocator that made it
[[nodiscard]] TRS_API
trs_Status trs_deque_move_assign(trs_Deque *self, trs_Deque *other);

/// writes every elem into 'dst' in ring order — at most two memcpy, the contents being at
/// most two runs. This is how a deque reaches algo: sort or search the copy, not the ring
/// @param self the deque
/// @param dst must have the same elem_size and be exactly as long as the deque
/// @bigo{n}
TRS_API
void trs_deque_copy_to_span(const trs_Deque *self, trs_SpanMut dst);

/// overwrites every elem from 'src' — the pair to trs_deque_copy_to_span: take the
/// contents out, hand them to algo, put the answer back
/// @param self the deque
/// @param src must have the same elem_size and be exactly as long as the deque. Nothing
///            is allocated, so nothing can fail
/// @bigo{n}
TRS_API
void trs_deque_copy_from_span(trs_Deque *self, trs_Span src);

/// @}

/// @name compare
/// @{

/// whether the two hold the same elems in ring order, byte for byte
/// @param a one deque
/// @param b must have the same elem_size — a mismatch there is a programmer error, not a
///          false; a differing length is just false
/// @return whether the lengths match and the elems do, wherever either ring starts
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_deque_eq(const trs_Deque *a, const trs_Deque *b);

/// whether the two hold equal elems under 'eq'
/// @param a one deque
/// @param b must have the same elem_size as 'a'
/// @param eq asked of every pair until one says no
/// @return whether the lengths match and every pair does
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_deque_eq_by(const trs_Deque *a, const trs_Deque *b, trs_Eq eq);

/// @}

/// @name info
/// @{

/// how many elems the deque holds
/// @param self the deque
/// @return the length, never above the capacity
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_deque_len(const trs_Deque *self);

/// how many elems fit before the block must grow
/// @param self the deque
/// @return the capacity; it says nothing about where the ring starts
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_deque_cap(const trs_Deque *self);

/// the size of one elem, as named at construction
/// @param self the deque
/// @return elem_size, which never moves
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_deque_elem_size(const trs_Deque *self);

/// how many bytes the elems take
/// @param self the deque
/// @return len * elem_size, the capacity not counted
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_deque_bytes(const trs_Deque *self);

/// the allocator the deque was built with
/// @param self the deque
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] TRS_API
trs_Al *trs_deque_al(const trs_Deque *self);

/// @}

/// @name access
/// @{

/// the front elem
/// @param self asserts the deque is not empty
/// @return a pointer into the block, good until the next op that may reallocate
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_deque_front(const trs_Deque *self);

/// the front elem, to write through
/// @copydetails trs_deque_front
[[nodiscard]] TRS_API
void *trs_deque_front_mut(trs_Deque *self);

/// the back elem
/// @copydetails trs_deque_front
[[nodiscard]] TRS_API
const void *trs_deque_back(const trs_Deque *self);

/// the back elem, to write through
/// @copydetails trs_deque_front
[[nodiscard]] TRS_API
void *trs_deque_back_mut(trs_Deque *self);

/// the elem at 'idx'
/// @param self the deque
/// @param idx counts from the front, so 0 is the front elem wherever the ring starts;
///            asserts idx < len
/// @return a pointer into the block, good until the next op that may reallocate
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_deque_get(const trs_Deque *self, size_t idx);

/// the elem at 'idx', to write through
/// @copydetails trs_deque_get
[[nodiscard]] TRS_API
void *trs_deque_get_mut(trs_Deque *self, size_t idx);

/// overwrites the elem at 'idx' with a copy of 'val'
/// @param self the deque
/// @param idx counts from the front; asserts idx < len
/// @param val the elem to copy in
/// @bigo{1}
TRS_API
void trs_deque_set(trs_Deque *self, size_t idx, const void *val);

/// @}

/// @name mods
/// @{

/// puts a copy of 'val' at the front
/// @param self the deque
/// @param val must not point into this deque's own block: a push that grows moves the
///            elems out from under it
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{1} amortized
[[nodiscard]] TRS_API
trs_Status trs_deque_push_front(trs_Deque *self, const void *val);

/// puts a copy of 'val' at the back
/// @copydetails trs_deque_push_front
[[nodiscard]] TRS_API
trs_Status trs_deque_push_back(trs_Deque *self, const void *val);

/// drops the front elem, keeping the capacity
/// @param self asserts the deque is not empty
/// @bigo{1}
TRS_API
void trs_deque_pop_front(trs_Deque *self);

/// drops the back elem, keeping the capacity
/// @copydetails trs_deque_pop_front
TRS_API
void trs_deque_pop_back(trs_Deque *self);

/// puts a copy of 'val' at 'idx', shifting whichever side is shorter
/// @param self the deque
/// @param idx asserts idx <= len; idx == len is push_back
/// @param val must not point into this deque's own block, as in trs_deque_push_front
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n} — half the constant of a vec, but still O(n): the two ends are what the type
///            is for
[[nodiscard]] TRS_API
trs_Status trs_deque_insert(trs_Deque *self, size_t idx, const void *val);

/// drops the elem at 'idx', closing the gap from whichever side is shorter
/// @param self the deque
/// @param idx asserts idx < len
/// @bigo{n}
TRS_API
void trs_deque_remove(trs_Deque *self, size_t idx);

/// drops every elem, keeping the block
/// @param self the deque
/// @bigo{1}
TRS_API
void trs_deque_clear(trs_Deque *self);

/// makes room for 'new_cap' elems, unrolling the ring into the new block
/// @param self the deque
/// @param new_cap a capacity at or below the one it has is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow, or new_cap * elem_size
///         overflows
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_reserve(trs_Deque *self, size_t new_cap);

/// gives back the room above the length
/// @param self a length of 0 releases the block outright
/// @retval TRS_STATUS_OK on success, and when there was nothing to give back
/// @retval TRS_STATUS_ERR_NO_MEM when the allocator refuses the smaller block, leaving
///         the deque as it was
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_shrink_to_fit(trs_Deque *self);

/// moves the length to 'new_len' at the BACK, so the front stays put and a resize never
/// renumbers what was already there
/// @param self the deque
/// @param new_len below the length drops from the back; above it appends zeroed elems
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_deque_resize(trs_Deque *self, size_t new_len);

/// exchanges the two deques whole, rings and all
/// @param[in,out] self one deque
/// @param[in,out] other must have the same elem_size and the same allocator: the blocks
///                      change hands where they lie, ring and all, so nothing is copied and
///                      nothing can fail. 'self' == 'other' is a no-op
/// @note two allocators are a broken precondition, not a runtime state — a block belongs to
///       the allocator that made it. To exchange across two, build each side on the other's
///       allocator with trs_deque_copy_with and hand the results over with
///       trs_deque_move_assign
/// @bigo{1}
TRS_API
void trs_deque_swap(trs_Deque *self, trs_Deque *other);

/// exchanges the elems at 'i' and 'j'
/// @param self the deque
/// @param i counts from the front; asserts i < len
/// @param j counts from the front; asserts j < len; i == j is a no-op
/// @bigo{1}
TRS_API
void trs_deque_swap_elems(trs_Deque *self, size_t i, size_t j);

/// @}

/// @name print
/// @{

/// writes the elems to a stream in ring order as [a, b, c], followed by a newline
/// @param self the deque
/// @param stream where to write
/// @param fprint the printer, called once per elem
/// @bigo{n}
TRS_API
void trs_deque_fprint(const trs_Deque *self, FILE *stream, trs_FPrint fprint);

/// trs_deque_fprint to stdout
/// @param self the deque
/// @param fprint the printer, called once per elem
/// @bigo{n}
TRS_API
void trs_deque_print(const trs_Deque *self, trs_FPrint fprint);

/// @}

/// @name macros
/// @{

/// trs_deque_new with sizeof(T) for the elem size
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new deque is written
/// @bigo{1}
#define TRS_DEQUE_NEW(T, al, out) \
    trs_deque_new(sizeof(T), (al), (out))

/// trs_deque_new_len with sizeof(T)
/// @param T the elem type
/// @param len how many zeroed elems
/// @param al the allocator
/// @param[out] out where the new deque is written
/// @bigo{n}
#define TRS_DEQUE_NEW_LEN(T, len, al, out) \
    trs_deque_new_len((len), sizeof(T), (al), (out))

/// trs_deque_new_cap with sizeof(T)
/// @param T the elem type
/// @param cap how many elems to make room for
/// @param al the allocator
/// @param[out] out where the new deque is written
/// @bigo{n}
#define TRS_DEQUE_NEW_CAP(T, cap, al, out) \
    trs_deque_new_cap((cap), sizeof(T), (al), (out))

/// trs_deque_from_data with sizeof(T)
/// @param T the elem type
/// @param data the elems to copy in, made to typecheck as a const T *
/// @param len how many elems to read from 'data'
/// @param al the allocator
/// @param[out] out where the new deque is written
/// @bigo{n}
#define TRS_DEQUE_FROM_DATA(T, data, len, al, out) \
    trs_deque_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

/// a new deque from the elems written out: TRS_DEQUE_OF(int32_t, al, &d, 5, 3, 1)
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new deque is written
/// @param ... the elems, front to back, as a T initializer list
/// @bigo{n}
#define TRS_DEQUE_OF(T, al, out, ...)                   \
    trs_deque_from_data(                                \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

/// trs_deque_front as a const T *
/// @param T the elem type
/// @param self the deque
/// @bigo{1}
#define TRS_DEQUE_FRONT_AS(T, self) \
    ((const T *) trs_deque_front((self)))

/// trs_deque_front_mut as a T *
/// @copydetails TRS_DEQUE_FRONT_AS
#define TRS_DEQUE_FRONT_MUT_AS(T, self) \
    ((T *) trs_deque_front_mut((self)))

/// trs_deque_back as a const T *
/// @copydetails TRS_DEQUE_FRONT_AS
#define TRS_DEQUE_BACK_AS(T, self) \
    ((const T *) trs_deque_back((self)))

/// trs_deque_back_mut as a T *
/// @copydetails TRS_DEQUE_FRONT_AS
#define TRS_DEQUE_BACK_MUT_AS(T, self) \
    ((T *) trs_deque_back_mut((self)))

/// walks the deque front to back, binding 'elem' to each elem in turn. A ring has no
/// view to hand out, so this is the walk that copies nothing
/// @param T the elem type
/// @param elem the name the loop variable takes; it is a const T *
/// @param self the deque
/// @note the length is read on every step, so pushing or popping inside the body changes
///       what the walk covers — and a growth moves the elems out from under 'elem'
/// @bigo{n} over the whole walk
#define TRS_DEQUE_FOR_EACH_AS(T, elem, self)                                       \
    for (size_t idx_ = 0, step_ = 0; idx_ < trs_deque_len(self); ++idx_, step_ = 0) \
        for (const T *elem = TRS_DEQUE_GET_AS(T, (self), idx_); !step_; step_ = 1)

/// the same walk over elems that may be written through
/// @copydetails TRS_DEQUE_FOR_EACH_AS
#define TRS_DEQUE_FOR_EACH_MUT_AS(T, elem, self)                                   \
    for (size_t idx_ = 0, step_ = 0; idx_ < trs_deque_len(self); ++idx_, step_ = 0) \
        for (T *elem = TRS_DEQUE_GET_MUT_AS(T, (self), idx_); !step_; step_ = 1)

/// trs_deque_get as a const T *
/// @param T the elem type
/// @param self the deque
/// @param idx the index, counted from the front
/// @bigo{1}
#define TRS_DEQUE_GET_AS(T, self, idx) \
    ((const T *) trs_deque_get((self), (idx)))

/// trs_deque_get_mut as a T *
/// @copydetails TRS_DEQUE_GET_AS
#define TRS_DEQUE_GET_MUT_AS(T, self, idx) \
    ((T *) trs_deque_get_mut((self), (idx)))

/// trs_deque_set from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the deque
/// @param idx the index, counted from the front
/// @param val the value to copy in
/// @bigo{1}
#define TRS_DEQUE_SET(T, self, idx, val) \
    trs_deque_set((self), (idx), &(T){ (val) })

/// trs_deque_push_front from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the deque
/// @param val the value to copy in
/// @bigo{1} amortized
#define TRS_DEQUE_PUSH_FRONT(T, self, val) \
    trs_deque_push_front((self), &(T){ (val) })

/// trs_deque_push_back from a value rather than an address
/// @copydetails TRS_DEQUE_PUSH_FRONT
#define TRS_DEQUE_PUSH_BACK(T, self, val) \
    trs_deque_push_back((self), &(T){ (val) })

/// trs_deque_insert from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the deque
/// @param idx the index, counted from the front
/// @param val the value to copy in
/// @bigo{n}
#define TRS_DEQUE_INSERT(T, self, idx, val) \
    trs_deque_insert((self), (idx), &(T){ (val) })

/// @}

/// @}
