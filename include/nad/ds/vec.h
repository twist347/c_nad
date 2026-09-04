#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// @file

/// @defgroup ds_vec ds/vec
/// @ingroup ds
/// @brief nad_Vec — an owning array that grows
///
/// The elems live in one contiguous block: a length says how much of it holds elems, a
/// capacity how much of it there is. The block doubles when it fills, so a push costs
/// O(1) amortized. Those two numbers are the whole difference from ds/arr — what a vec
/// adds is what only means something when the block can change size.
///
/// Growing moves the elems, so a pointer from nad_vec_get_mut, a view from
/// nad_vec_to_span_mut and the block from nad_vec_data are good only until the next
/// operation that may reallocate: push, insert, extend, insert_span, reserve, resize,
/// shrink_to_fit, swap. Stable positions are what ds/list is for.
///
/// The bridge to algo runs both ways — nad_vec_to_span_mut hands the elems over to be
/// sorted or filled in place, nad_vec_resize adopts the length the algorithm leaves
/// behind. A vec keeps no order of its own to protect, unlike ds/stack and ds/queue.
///
/// An index out of range asserts; the ops that return a nad_Status are the ones that
/// allocate.
///
/// @par Example
/// @snippet ds/example_vec.c build
/// @snippet ds/example_vec.c grow
/// @snippet ds/example_vec.c bulk
/// @snippet ds/example_vec.c algo
/// @{

/// Owning array that grows.
/// An opaque handle: it comes from a constructor and goes back to nad_vec_drop
typedef struct nad_Vec nad_Vec;

/// @name lifetime
/// @{

/// an empty vec that owns no block yet
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator, kept for everything after
/// @param[out] out the new vec, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header cannot be allocated
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Status nad_vec_new(size_t elem_size, nad_Al *al, nad_Vec **out);

/// a vec of 'len' zeroed elems, with the capacity to match
/// @param len how many elems
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new vec, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_new_len(size_t len, size_t elem_size, nad_Al *al, nad_Vec **out);

/// an empty vec with room for 'cap' elems before the first growth
/// @param cap how many elems to make room for
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new vec, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         cap * elem_size overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Vec **out);

/// a vec holding a copy of 'len' elems read from 'data'
/// @param data the elems to copy in; may be null only when len is 0
/// @param len how many elems to read
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new vec, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Vec **out);

/// a vec holding a copy of what 's' views, taking its elem_size
/// @param s the view to copy
/// @param al the allocator
/// @param[out] out the new vec, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_from_span(nad_Span s, nad_Al *al, nad_Vec **out);

/// releases the block and the vec through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object
/// @bigo{1}
NAD_API
void nad_vec_drop(nad_Vec *self);

/// @}

/// @name copy
/// @{

/// a new vec with the same elems, on the same allocator
/// @param self the vec to copy
/// @param[out] out the new vec, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_copy(const nad_Vec *self, nad_Vec **out);

/// a new vec with the same elems, on 'al'
/// @param self the vec to copy
/// @param al where the copy lives; nad_vec_copy is this one with the allocator of 'self'
/// @param[out] out the new vec, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_copy_with(const nad_Vec *self, nad_Al *al, nad_Vec **out);

/// overwrites the elems of 'other' with those of 'self', growing its block when it must
/// @param self the vec to copy from
/// @param[in,out] other must have the same elem_size; keeps its own allocator, and
///                      'self' == 'other' is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow, leaving 'other' as it was
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_copy_assign(const nad_Vec *self, nad_Vec *other);

/// moves the elems of 'self' into 'other', leaving 'self' empty
/// @param[in,out] self the vec to move from; emptied on success and still usable, on
///                     its own allocator
/// @param[in,out] other must have the same elem_size; releases what it held and keeps its own allocator. 'self' == 'other' is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the two sit on different allocators and the block cannot be taken,
///         leaving both as they were
/// @bigo{1} on one allocator, n on two — the block belongs to the allocator that made it
[[nodiscard]] NAD_API
nad_Status nad_vec_move_assign(nad_Vec *self, nad_Vec *other);

/// @}

/// @name compare
/// @{

/// whether the two hold the same elems, byte for byte
/// @param a one vec
/// @param b must have the same elem_size — a mismatch there is a programmer error, not a
///          false; a differing length is just false
/// @return whether the lengths match and the bytes do
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_vec_eq(const nad_Vec *a, const nad_Vec *b);

/// whether the two hold equal elems under 'eq'
/// @param a one vec
/// @param b must have the same elem_size as 'a'
/// @param eq asked of every pair until one says no
/// @return whether the lengths match and every pair does
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_vec_eq_by(const nad_Vec *a, const nad_Vec *b, nad_Eq eq);

/// @}

/// @name info
/// @{

/// how many elems the vec holds
/// @param self the vec
/// @return the length, never above the capacity
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_vec_len(const nad_Vec *self);

/// how many elems fit before the block must grow
/// @param self the vec
/// @return the capacity
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_vec_cap(const nad_Vec *self);

/// the size of one elem, as named at construction
/// @param self the vec
/// @return elem_size, which never moves
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_vec_elem_size(const nad_Vec *self);

/// how many bytes the elems take
/// @param self the vec
/// @return len * elem_size, the capacity not counted
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_vec_bytes(const nad_Vec *self);

/// the allocator the vec was built with
/// @param self the vec
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_vec_al(const nad_Vec *self);

/// @}

/// @name access
/// @{

/// the first elem
/// @param self asserts the vec is not empty
/// @return a pointer into the block, good until the next op that may reallocate
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_vec_first(const nad_Vec *self);

/// the first elem, to write through
/// @copydetails nad_vec_first
[[nodiscard]] NAD_API
void *nad_vec_first_mut(nad_Vec *self);

/// the last elem
/// @copydetails nad_vec_first
[[nodiscard]] NAD_API
const void *nad_vec_last(const nad_Vec *self);

/// the last elem, to write through
/// @copydetails nad_vec_first
[[nodiscard]] NAD_API
void *nad_vec_last_mut(nad_Vec *self);

/// the elem at 'idx'
/// @param self the vec
/// @param idx asserts idx < len
/// @return a pointer into the block, good until the next op that may reallocate
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_vec_get(const nad_Vec *self, size_t idx);

/// the elem at 'idx', to write through
/// @copydetails nad_vec_get
[[nodiscard]] NAD_API
void *nad_vec_get_mut(nad_Vec *self, size_t idx);

/// overwrites the elem at 'idx' with a copy of 'val'
/// @param self the vec
/// @param idx asserts idx < len
/// @param val the elem to copy in
/// @bigo{1}
NAD_API
void nad_vec_set(nad_Vec *self, size_t idx, const void *val);

/// the block itself
/// @param self the vec
/// @return the first elem, or null while the vec owns no block; good until the next op
///         that may reallocate
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_vec_data(const nad_Vec *self);

/// the block itself, to write through
/// @copydetails nad_vec_data
[[nodiscard]] NAD_API
void *nad_vec_data_mut(nad_Vec *self);

/// @}

/// @name mods
/// @{

/// appends a copy of 'val'
/// @param self the vec
/// @param val must not point into this vec's own block: a push that grows moves the elems
///            out from under it
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{1} amortized
[[nodiscard]] NAD_API
nad_Status nad_vec_push(nad_Vec *self, const void *val);

/// drops the last elem, keeping the capacity
/// @param self asserts the vec is not empty
/// @bigo{1}
NAD_API
void nad_vec_pop(nad_Vec *self);

/// puts a copy of 'val' at 'idx', moving the elems from there on one place up
/// @param self the vec
/// @param idx asserts idx <= len; idx == len appends
/// @param val must not point into this vec's own block, as in nad_vec_push
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_insert(nad_Vec *self, size_t idx, const void *val);

/// drops the elem at 'idx' and closes the gap
/// @param self the vec
/// @param idx asserts idx < len
/// @bigo{n}
NAD_API
void nad_vec_remove(nad_Vec *self, size_t idx);

/// drops every elem, keeping the block
/// @param self the vec
/// @bigo{1}
NAD_API
void nad_vec_clear(nad_Vec *self);

/// makes room for 'new_cap' elems
/// @param self the vec
/// @param new_cap a capacity at or below the one it has is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow, or new_cap * elem_size
///         overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_reserve(nad_Vec *self, size_t new_cap);

/// gives back the room above the length
/// @param self a length of 0 releases the block outright
/// @retval NAD_STATUS_OK on success, and when there was nothing to give back
/// @retval NAD_STATUS_ERR_NO_MEM when the allocator refuses the smaller block, leaving
///         the vec as it was
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_shrink_to_fit(nad_Vec *self);

/// moves the length to 'new_len', zeroing what it grows over
/// @param self the vec
/// @param new_len below the length drops the tail; above it appends zeroed elems
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_vec_resize(nad_Vec *self, size_t new_len);

/// exchanges the contents of the two
/// @param self one vec
/// @param other must have the same elem_size
/// @retval NAD_STATUS_OK on success; on one allocator the blocks are handed over and the
///         capacity travels with them, on two the bytes are moved and each side is left
///         sized to its new content
/// @retval NAD_STATUS_ERR_NO_MEM when the two sit on different allocators and a block
///         cannot be taken, leaving both as they were
/// @bigo{1} on one allocator, n on two
[[nodiscard]] NAD_API
nad_Status nad_vec_swap(nad_Vec *self, nad_Vec *other);

/// exchanges the elems at 'i' and 'j'
/// @param self the vec
/// @param i asserts i < len
/// @param j asserts j < len; i == j is a no-op
/// @bigo{1}
NAD_API
void nad_vec_swap_elems(nad_Vec *self, size_t i, size_t j);

/// @}

/// @name bulk mods
/// @{

/// appends every elem of 'src', in order
/// @param self the vec
/// @param src must have the same elem_size, and must not view this vec's own block:
///            growing frees what 'src' would be reading from
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n} — the room is taken once for the whole run and with the growth factor a push
///            uses, so a run of extends stays amortized O(1) per elem
[[nodiscard]] NAD_API
nad_Status nad_vec_extend(nad_Vec *self, nad_Span src);

/// inserts every elem of 'src' before 'idx', in order
/// @param self the vec
/// @param idx asserts idx <= len; idx == len extends
/// @param src must have the same elem_size, and must not view this vec's own block, as in
///            nad_vec_extend
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot grow
/// @bigo{n} — the tail moves once for the whole run, which a loop of insert cannot do: it
///            moves the tail once per elem and costs O(len * src.len)
[[nodiscard]] NAD_API
nad_Status nad_vec_insert_span(nad_Vec *self, size_t idx, nad_Span src);

/// drops 'count' elems starting at 'idx' and closes the gap, moving the tail once
/// @param self the vec
/// @param idx asserts idx <= len
/// @param count asserts idx + count <= len; 0 does nothing. Nothing is allocated, so
///              nothing can fail
/// @bigo{n}
NAD_API
void nad_vec_remove_range(nad_Vec *self, size_t idx, size_t count);

/// @}

/// @name to span
/// @{

/// a writable view of the elems, the way in to algo
/// @param self the vec
/// @return a view good until the next op that may reallocate
/// @bigo{1}
[[nodiscard]] NAD_API
nad_SpanMut nad_vec_to_span_mut(nad_Vec *self);

/// a read-only view of the elems
/// @copydetails nad_vec_to_span_mut
[[nodiscard]] NAD_API
nad_Span nad_vec_to_span(const nad_Vec *self);

/// @}

/// @name print
/// @{

/// writes the elems to a stream as [a, b, c], followed by a newline
/// @param self the vec
/// @param stream where to write
/// @param fprint the printer, called once per elem
/// @bigo{n}
NAD_API
void nad_vec_fprint(const nad_Vec *self, FILE *stream, nad_FPrint fprint);

/// nad_vec_fprint to stdout
/// @param self the vec
/// @param fprint the printer, called once per elem
/// @bigo{n}
NAD_API
void nad_vec_print(const nad_Vec *self, nad_FPrint fprint);

/// @}

/// @name macros
/// @{

/// nad_vec_new with sizeof(T) for the elem size
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @bigo{1}
#define NAD_VEC_NEW(T, al, out) \
    nad_vec_new(sizeof(T), (al), (out))

/// nad_vec_new_len with sizeof(T)
/// @param T the elem type
/// @param len how many zeroed elems
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @bigo{n}
#define NAD_VEC_NEW_LEN(T, len, al, out) \
    nad_vec_new_len((len), sizeof(T), (al), (out))

/// nad_vec_new_cap with sizeof(T)
/// @param T the elem type
/// @param cap how many elems to make room for
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @bigo{n}
#define NAD_VEC_NEW_CAP(T, cap, al, out) \
    nad_vec_new_cap((cap), sizeof(T), (al), (out))

/// nad_vec_from_data with sizeof(T)
/// @param T the elem type
/// @param data the elems to copy in, made to typecheck as a const T *
/// @param len how many elems to read from 'data'
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @bigo{n}
#define NAD_VEC_FROM_DATA(T, data, len, al, out) \
    nad_vec_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

/// a new vec from the elems written out: NAD_VEC_OF(int32_t, al, &v, 5, 3, 1)
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new vec is written
/// @param ... the elems, as a T initializer list
/// @bigo{n}
#define NAD_VEC_OF(T, al, out, ...)                     \
    nad_vec_from_data(                                  \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

/// nad_vec_first as a const T *
/// @param T the elem type
/// @param self the vec
/// @bigo{1}
#define NAD_VEC_FIRST_AS(T, self) \
    ((const T *) nad_vec_first((self)))

/// nad_vec_first_mut as a T *
/// @copydetails NAD_VEC_FIRST_AS
#define NAD_VEC_FIRST_MUT_AS(T, self) \
    ((T *) nad_vec_first_mut((self)))

/// nad_vec_last as a const T *
/// @copydetails NAD_VEC_FIRST_AS
#define NAD_VEC_LAST_AS(T, self) \
    ((const T *) nad_vec_last((self)))

/// nad_vec_last_mut as a T *
/// @copydetails NAD_VEC_FIRST_AS
#define NAD_VEC_LAST_MUT_AS(T, self) \
    ((T *) nad_vec_last_mut((self)))

/// nad_vec_get as a const T *
/// @param T the elem type
/// @param self the vec
/// @param idx the index
/// @bigo{1}
#define NAD_VEC_GET_AS(T, self, idx) \
    ((const T *) nad_vec_get((self), (idx)))

/// nad_vec_get_mut as a T *
/// @copydetails NAD_VEC_GET_AS
#define NAD_VEC_GET_MUT_AS(T, self, idx) \
    ((T *) nad_vec_get_mut((self), (idx)))

/// nad_vec_set from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the vec
/// @param idx the index
/// @param val the value to copy in
/// @bigo{1}
#define NAD_VEC_SET(T, self, idx, val) \
    nad_vec_set((self), (idx), &(T){ (val) })

/// nad_vec_push from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the vec
/// @param val the value to copy in
/// @bigo{1} amortized
#define NAD_VEC_PUSH(T, self, val) \
    nad_vec_push((self), &(T){ (val) })

/// nad_vec_insert from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the vec
/// @param idx the index
/// @param val the value to copy in
/// @bigo{n}
#define NAD_VEC_INSERT(T, self, idx, val) \
    nad_vec_insert((self), (idx), &(T){ (val) })

/// nad_vec_extend from the elems written out
/// @param T the elem type
/// @param self the vec
/// @param ... the elems, as a T initializer list
/// @bigo{n}
#define NAD_VEC_EXTEND(T, self, ...) \
    nad_vec_extend((self), NAD_SPAN_OF(T, __VA_ARGS__))

/// nad_vec_insert_span from the elems written out
/// @param T the elem type
/// @param self the vec
/// @param idx the index
/// @param ... the elems, as a T initializer list
/// @bigo{n}
#define NAD_VEC_INSERT_SPAN(T, self, idx, ...) \
    nad_vec_insert_span((self), (idx), NAD_SPAN_OF(T, __VA_ARGS__))

/// @}

/// @}
