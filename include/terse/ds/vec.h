#pragma once

#include "terse/alloc/alloc.h"
#include "terse/core/cmp.h"
#include "terse/core/export.h"
#include "terse/core/span.h"
#include "terse/core/status.h"

#include <stddef.h>

/// @file

/// @defgroup ds_vec ds/vec
/// @ingroup ds
/// @brief trs_Vec — an owning array that grows
///
/// The elems live in one contiguous block: a length says how much of it holds elems, a
/// capacity how much of it there is. The block doubles when it fills, so a push costs
/// O(1) amortized. Those two numbers are the whole difference from ds/arr — what a vec
/// adds is what only means something when the block can change size.
///
/// Growing moves the elems, so a pointer from trs_vec_get_mut, a view from
/// trs_vec_to_span_mut and the block from trs_vec_data are good only until the next
/// operation that may reallocate: push, insert, extend, insert_span, reserve, resize,
/// shrink_to_fit, swap. Stable positions are what ds/list is for.
///
/// The bridge to algo runs both ways — trs_vec_to_span_mut hands the elems over to be
/// sorted or filled in place, trs_vec_resize adopts the length the algorithm leaves
/// behind. A vec keeps no order of its own to protect, unlike ds/stack and ds/queue.
///
/// An index out of range asserts; the ops that return a trs_Status are the ones that
/// allocate.
///
/// @par Example
/// @snippet ds/example_vec.c build
/// @snippet ds/example_vec.c grow
/// @snippet ds/example_vec.c bulk
/// @snippet ds/example_vec.c algo
/// @snippet ds/example_vec.c allocators
/// @{

/// Owning array that grows.
/// An opaque handle: it comes from a constructor and goes back to trs_vec_drop
typedef struct trs_Vec trs_Vec;

/// @name lifetime
/// @{

/// an empty vec that owns no block yet
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator, kept for everything after
/// @param[out] out the new vec, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header cannot be allocated
/// @bigo{1}
[[nodiscard]] TRS_API
trs_Status trs_vec_new(size_t elem_size, trs_Al *al, trs_Vec **out);

/// a vec of 'len' zeroed elems, with the capacity to match
/// @param len how many elems
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new vec, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_new_len(size_t len, size_t elem_size, trs_Al *al, trs_Vec **out);

/// an empty vec with room for 'cap' elems before the first growth
/// @param cap how many elems to make room for
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new vec, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         cap * elem_size overflows
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_new_cap(size_t cap, size_t elem_size, trs_Al *al, trs_Vec **out);

/// a vec holding a copy of 'len' elems read from 'data'
/// @param data the elems to copy in; may be null only when len is 0
/// @param len how many elems to read
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new vec, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_from_data(const void *data, size_t len, size_t elem_size, trs_Al *al, trs_Vec **out);

/// a vec holding a copy of what 's' views, taking its elem_size
/// @param s the view to copy
/// @param al the allocator
/// @param[out] out the new vec, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_from_span(trs_Span s, trs_Al *al, trs_Vec **out);

/// releases the block and the vec through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object
/// @bigo{1}
TRS_API
void trs_vec_drop(trs_Vec *self);

/// @}

/// @name copy
/// @{

/// a new vec with the same elems, on the same allocator
/// @param self the vec to copy
/// @param[out] out the new vec, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_copy(const trs_Vec *self, trs_Vec **out);

/// a new vec with the same elems, on 'al'
/// @param self the vec to copy
/// @param al where the copy lives; trs_vec_copy is this one with the allocator of 'self'
/// @param[out] out the new vec, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_copy_with(const trs_Vec *self, trs_Al *al, trs_Vec **out);

/// overwrites the elems of 'other' with those of 'self', growing its block when it must
/// @param self the vec to copy from
/// @param[in,out] other must have the same elem_size; keeps its own allocator, and
///                      'self' == 'other' is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow, leaving 'other' as it was
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_copy_assign(const trs_Vec *self, trs_Vec *other);

/// moves the elems of 'self' into 'other', leaving 'self' empty
/// @param[in,out] self the vec to move from; emptied on success and still usable, on
///                     its own allocator
/// @param[in,out] other must have the same elem_size; releases what it held and keeps its own allocator. 'self' == 'other' is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the two sit on different allocators and the block cannot be taken,
///         leaving both as they were
/// @bigo{1} on one allocator, n on two — the block belongs to the allocator that made it
[[nodiscard]] TRS_API
trs_Status trs_vec_move_assign(trs_Vec *self, trs_Vec *other);

/// @}

/// @name compare
/// @{

/// whether the two hold the same elems, byte for byte
/// @param a one vec
/// @param b must have the same elem_size — a mismatch there is a programmer error, not a
///          false; a differing length is just false
/// @return whether the lengths match and the bytes do
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_vec_eq(const trs_Vec *a, const trs_Vec *b);

/// whether the two hold equal elems under 'eq'
/// @param a one vec
/// @param b must have the same elem_size as 'a'
/// @param eq asked of every pair until one says no
/// @return whether the lengths match and every pair does
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_vec_eq_by(const trs_Vec *a, const trs_Vec *b, trs_Eq eq);

/// @}

/// @name info
/// @{

/// how many elems the vec holds
/// @param self the vec
/// @return the length, never above the capacity
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_vec_len(const trs_Vec *self);

/// how many elems fit before the block must grow
/// @param self the vec
/// @return the capacity
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_vec_cap(const trs_Vec *self);

/// the size of one elem, as named at construction
/// @param self the vec
/// @return elem_size, which never moves
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_vec_elem_size(const trs_Vec *self);

/// how many bytes the elems take
/// @param self the vec
/// @return len * elem_size, the capacity not counted
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_vec_bytes(const trs_Vec *self);

/// the allocator the vec was built with
/// @param self the vec
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] TRS_API
trs_Al *trs_vec_al(const trs_Vec *self);

/// @}

/// @name access
/// @{

/// the front elem
/// @param self asserts the vec is not empty
/// @return a pointer into the block, good until the next op that may reallocate
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_vec_front(const trs_Vec *self);

/// the front elem, to write through
/// @copydetails trs_vec_front
[[nodiscard]] TRS_API
void *trs_vec_front_mut(trs_Vec *self);

/// the back elem
/// @copydetails trs_vec_front
[[nodiscard]] TRS_API
const void *trs_vec_back(const trs_Vec *self);

/// the back elem, to write through
/// @copydetails trs_vec_front
[[nodiscard]] TRS_API
void *trs_vec_back_mut(trs_Vec *self);

/// the elem at 'idx'
/// @param self the vec
/// @param idx asserts idx < len
/// @return a pointer into the block, good until the next op that may reallocate
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_vec_get(const trs_Vec *self, size_t idx);

/// the elem at 'idx', to write through
/// @copydetails trs_vec_get
[[nodiscard]] TRS_API
void *trs_vec_get_mut(trs_Vec *self, size_t idx);

/// overwrites the elem at 'idx' with a copy of 'val'
/// @param self the vec
/// @param idx asserts idx < len
/// @param val the elem to copy in
/// @bigo{1}
TRS_API
void trs_vec_set(trs_Vec *self, size_t idx, const void *val);

/// the block itself
/// @param self the vec
/// @return the first elem, or null while the vec owns no block; good until the next op
///         that may reallocate
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_vec_data(const trs_Vec *self);

/// the block itself, to write through
/// @copydetails trs_vec_data
[[nodiscard]] TRS_API
void *trs_vec_data_mut(trs_Vec *self);

/// @}

/// @name mods
/// @{

/// appends a copy of 'val'
/// @param self the vec
/// @param val must not point into this vec's own block: a push that grows moves the elems
///            out from under it
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{1} amortized
[[nodiscard]] TRS_API
trs_Status trs_vec_push(trs_Vec *self, const void *val);

/// drops the back elem, keeping the capacity
/// @param self asserts the vec is not empty
/// @bigo{1}
TRS_API
void trs_vec_pop(trs_Vec *self);

/// puts a copy of 'val' at 'idx', moving the elems from there on one place up
/// @param self the vec
/// @param idx asserts idx <= len; idx == len appends
/// @param val must not point into this vec's own block, as in trs_vec_push
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_insert(trs_Vec *self, size_t idx, const void *val);

/// drops the elem at 'idx' and closes the gap
/// @param self the vec
/// @param idx asserts idx < len
/// @bigo{n}
TRS_API
void trs_vec_remove(trs_Vec *self, size_t idx);

/// drops every elem, keeping the block
/// @param self the vec
/// @bigo{1}
TRS_API
void trs_vec_clear(trs_Vec *self);

/// makes room for 'new_cap' elems
/// @param self the vec
/// @param new_cap a capacity at or below the one it has is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow, or new_cap * elem_size
///         overflows
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_reserve(trs_Vec *self, size_t new_cap);

/// gives back the room above the length
/// @param self a length of 0 releases the block outright
/// @retval TRS_STATUS_OK on success, and when there was nothing to give back
/// @retval TRS_STATUS_ERR_NO_MEM when the allocator refuses the smaller block, leaving
///         the vec as it was
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_shrink_to_fit(trs_Vec *self);

/// moves the length to 'new_len', zeroing what it grows over
/// @param self the vec
/// @param new_len below the length drops the tail; above it appends zeroed elems
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_vec_resize(trs_Vec *self, size_t new_len);

/// exchanges the two vecs whole, lengths, capacities and all
/// @param[in,out] self one vec
/// @param[in,out] other must have the same elem_size and the same allocator: the blocks
///                      change hands where they lie, capacity and all, so nothing is copied
///                      and nothing can fail. 'self' == 'other' is a no-op
/// @note two allocators are a broken precondition, not a runtime state — a block belongs to
///       the allocator that made it. To exchange across two, build each side on the other's
///       allocator with trs_vec_copy_with and hand the results over with
///       trs_vec_move_assign
/// @bigo{1}
TRS_API
void trs_vec_swap(trs_Vec *self, trs_Vec *other);

/// exchanges the elems at 'i' and 'j'
/// @param self the vec
/// @param i asserts i < len
/// @param j asserts j < len; i == j is a no-op
/// @bigo{1}
TRS_API
void trs_vec_swap_elems(trs_Vec *self, size_t i, size_t j);

/// @}

/// @name bulk mods
/// @{

/// appends every elem of 'src', in order
/// @param self the vec
/// @param src must have the same elem_size, and must not view this vec's own block:
///            growing frees what 'src' would be reading from
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n} — the room is taken once for the whole run and with the growth factor a push
///            uses, so a run of extends stays amortized O(1) per elem
[[nodiscard]] TRS_API
trs_Status trs_vec_extend(trs_Vec *self, trs_Span src);

/// inserts every elem of 'src' before 'idx', in order
/// @param self the vec
/// @param idx asserts idx <= len; idx == len extends
/// @param src must have the same elem_size, and must not view this vec's own block, as in
///            trs_vec_extend
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n} — the tail moves once for the whole run, which a loop of insert cannot do: it
///            moves the tail once per elem and costs O(len * src.len)
[[nodiscard]] TRS_API
trs_Status trs_vec_insert_span(trs_Vec *self, size_t idx, trs_Span src);

/// drops 'count' elems starting at 'idx' and closes the gap, moving the tail once
/// @param self the vec
/// @param idx asserts idx <= len
/// @param count asserts idx + count <= len; 0 does nothing. Nothing is allocated, so
///              nothing can fail
/// @bigo{n}
TRS_API
void trs_vec_remove_range(trs_Vec *self, size_t idx, size_t count);

/// @}

/// @name to span
/// @{

/// a writable view of the elems, the way in to algo
/// @param self the vec
/// @return a view good until the next op that may reallocate
/// @bigo{1}
[[nodiscard]] TRS_API
trs_SpanMut trs_vec_to_span_mut(trs_Vec *self);

/// a read-only view of the elems
/// @copydetails trs_vec_to_span_mut
[[nodiscard]] TRS_API
trs_Span trs_vec_to_span(const trs_Vec *self);

/// @}

/// @name print
/// @{

/// writes the elems to a stream as [a, b, c], followed by a newline
/// @param self the vec
/// @param stream where to write
/// @param fprint the printer, called once per elem
/// @bigo{n}
TRS_API
void trs_vec_fprint(const trs_Vec *self, FILE *stream, trs_FPrint fprint);

/// trs_vec_fprint to stdout
/// @param self the vec
/// @param fprint the printer, called once per elem
/// @bigo{n}
TRS_API
void trs_vec_print(const trs_Vec *self, trs_FPrint fprint);

/// @}

/// @name macros
/// @{

/// trs_vec_new with sizeof(T) for the elem size
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @bigo{1}
#define TRS_VEC_NEW(T, al, out) \
    trs_vec_new(sizeof(T), (al), (out))

/// trs_vec_new_len with sizeof(T)
/// @param T the elem type
/// @param len how many zeroed elems
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @bigo{n}
#define TRS_VEC_NEW_LEN(T, len, al, out) \
    trs_vec_new_len((len), sizeof(T), (al), (out))

/// trs_vec_new_cap with sizeof(T)
/// @param T the elem type
/// @param cap how many elems to make room for
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @bigo{n}
#define TRS_VEC_NEW_CAP(T, cap, al, out) \
    trs_vec_new_cap((cap), sizeof(T), (al), (out))

/// trs_vec_from_data with sizeof(T)
/// @param T the elem type
/// @param data the elems to copy in, made to typecheck as a const T *
/// @param len how many elems to read from 'data'
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @bigo{n}
#define TRS_VEC_FROM_DATA(T, data, len, al, out) \
    trs_vec_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

/// a new vec from the elems written out: TRS_VEC_OF(int32_t, al, &v, 5, 3, 1)
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @param ... the elems, as a T initializer list
/// @bigo{n}
#define TRS_VEC_OF(T, al, out, ...)                     \
    trs_vec_from_data(                                  \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

/// trs_vec_front as a const T *
/// @param T the elem type
/// @param self the vec
/// @bigo{1}
#define TRS_VEC_FRONT_AS(T, self) \
    ((const T *) trs_vec_front((self)))

/// trs_vec_front_mut as a T *
/// @copydetails TRS_VEC_FRONT_AS
#define TRS_VEC_FRONT_MUT_AS(T, self) \
    ((T *) trs_vec_front_mut((self)))

/// trs_vec_back as a const T *
/// @copydetails TRS_VEC_FRONT_AS
#define TRS_VEC_BACK_AS(T, self) \
    ((const T *) trs_vec_back((self)))

/// trs_vec_back_mut as a T *
/// @copydetails TRS_VEC_FRONT_AS
#define TRS_VEC_BACK_MUT_AS(T, self) \
    ((T *) trs_vec_back_mut((self)))

/// trs_vec_get as a const T *
/// @param T the elem type
/// @param self the vec
/// @param idx the index
/// @bigo{1}
#define TRS_VEC_GET_AS(T, self, idx) \
    ((const T *) trs_vec_get((self), (idx)))

/// trs_vec_get_mut as a T *
/// @copydetails TRS_VEC_GET_AS
#define TRS_VEC_GET_MUT_AS(T, self, idx) \
    ((T *) trs_vec_get_mut((self), (idx)))

/// trs_vec_set from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the vec
/// @param idx the index
/// @param val the value to copy in
/// @bigo{1}
#define TRS_VEC_SET(T, self, idx, val) \
    trs_vec_set((self), (idx), &(T){ (val) })

/// trs_vec_push from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the vec
/// @param val the value to copy in
/// @bigo{1} amortized
#define TRS_VEC_PUSH(T, self, val) \
    trs_vec_push((self), &(T){ (val) })

/// trs_vec_insert from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the vec
/// @param idx the index
/// @param val the value to copy in
/// @bigo{n}
#define TRS_VEC_INSERT(T, self, idx, val) \
    trs_vec_insert((self), (idx), &(T){ (val) })

/// trs_vec_extend from the elems written out
/// @param T the elem type
/// @param self the vec
/// @param ... the elems, as a T initializer list
/// @bigo{n}
#define TRS_VEC_EXTEND(T, self, ...) \
    trs_vec_extend((self), TRS_SPAN_OF(T, __VA_ARGS__))

/// trs_vec_insert_span from the elems written out
/// @param T the elem type
/// @param self the vec
/// @param idx the index
/// @param ... the elems, as a T initializer list
/// @bigo{n}
#define TRS_VEC_INSERT_SPAN(T, self, idx, ...) \
    trs_vec_insert_span((self), (idx), TRS_SPAN_OF(T, __VA_ARGS__))

/// @}

/// @}
