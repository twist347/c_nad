#pragma once

#include "trs/alloc/alloc.h"
#include "trs/core/cmp.h"
#include "trs/core/export.h"
#include "trs/core/print.h"
#include "trs/core/span.h"
#include "trs/core/status.h"

#include <stddef.h>

/// @file

/// @defgroup ds_arr ds/arr
/// @ingroup ds
/// @brief trs_Arr — an owning array whose length is set when it is built
///
/// The elems live in one block, allocated once, and no operation changes how many there
/// are. This is ds/vec minus the growth.
///
/// The length moves only when the whole arr is replaced: trs_arr_copy_assign overwrites
/// one, trs_arr_swap exchanges two. Those are also the only two ops that move the block,
/// and so the only two that invalidate what trs_arr_get_mut, trs_arr_data and
/// trs_arr_to_span_mut handed out.
///
/// The view is writable, so algo sorts and fills in place through it. An index out of
/// range asserts; the ops that return a trs_Status are the ones that allocate.
///
/// An elem is bytes: the arr copies them in and frees them with the block. Whatever an
/// elem points to is the caller's to release.
///
/// The typed macros write 'const T', so an elem type already spelled with const needs a
/// typedef of its own.
///
/// @par Example
/// @snippet ds/example_arr.c build
/// @snippet ds/example_arr.c compare
/// @snippet ds/example_arr.c algo
/// @snippet ds/example_arr.c access
/// @snippet ds/example_arr.c copy
/// @{

/// Owning array whose length is set when it is built.
/// An opaque handle: it comes from one of the constructors and goes back to trs_arr_drop
typedef struct trs_Arr trs_Arr;

/// @name lifetime
/// @{

/// a new arr of 'len' zeroed elems
/// @param len how many elems the arr will hold; 0 gives an arr that owns no block
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator, kept for everything after
/// @param[out] out the new arr, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n} — the block is zeroed
[[nodiscard]] TRS_API
trs_Status trs_arr_new_len(size_t len, size_t elem_size, trs_Al *al, trs_Arr **out);

/// a new arr holding a copy of 'len' elems read from 'data'
/// @param data the elems to copy in; may be null only when len is 0
/// @param len how many elems to read
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new arr, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_arr_from_data(const void *data, size_t len, size_t elem_size, trs_Al *al, trs_Arr **out);

/// a new arr holding a copy of what 's' views, taking its len and elem_size
/// @param s the view to copy
/// @param al the allocator; unrelated to where 's' points, the elems are copied out of it
/// @param[out] out the new arr, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_arr_from_span(trs_Span s, trs_Al *al, trs_Arr **out);

/// releases the block and the arr through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object; what the elems
///             point to is not released
/// @bigo{1}
TRS_API
void trs_arr_drop(trs_Arr *self);

/// @}

/// @name copy
/// @{

/// a new arr with the same elems, on the same allocator
/// @param self the arr to copy
/// @param[out] out the new arr, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_arr_copy(const trs_Arr *self, trs_Arr **out);

/// a new arr with the same elems, on 'al'
/// @param self the arr to copy
/// @param al where the copy lives; trs_arr_copy is this one with the allocator of 'self'
/// @param[out] out the new arr, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_arr_copy_with(const trs_Arr *self, trs_Al *al, trs_Arr **out);

/// overwrites the elems of 'other' with those of 'self', resizing its block when the two
/// lengths differ
/// @param self the arr to copy from
/// @param[in,out] other must have the same elem_size; keeps its own allocator, and
///                      'self' == 'other' is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the block cannot be resized, leaving 'other' as
///         it was
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_arr_copy_assign(const trs_Arr *self, trs_Arr *other);

/// moves the elems of 'self' into 'other', leaving 'self' empty
/// @param[in,out] self the arr to move from; emptied on success and still usable, on
///                     its own allocator
/// @param[in,out] other must have the same elem_size; releases what it held and keeps its own allocator. 'self' == 'other' is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the two sit on different allocators and the block cannot be taken,
///         leaving both as they were
/// @bigo{1} on one allocator, n on two — the block belongs to the allocator that made it
[[nodiscard]] TRS_API
trs_Status trs_arr_move_assign(trs_Arr *self, trs_Arr *other);

/// @}

/// @name compare
/// @{

/// whether the two hold the same elems, byte for byte
/// @param a one arr
/// @param b must have the same elem_size — a mismatch there is a programmer error, not a
///          false; a differing length is just false
/// @return whether the lengths match and the bytes do; being memcmp, it parts -0.0 from
///         +0.0 and counts a struct's padding
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_arr_eq(const trs_Arr *a, const trs_Arr *b);

/// whether the two hold equal elems under 'eq'
/// @param a one arr
/// @param b must have the same elem_size as 'a'
/// @param eq asked of every pair until one says no
/// @return whether the lengths match and every pair does
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_arr_eq_by(const trs_Arr *a, const trs_Arr *b, trs_Eq eq);

/// @}

/// @name info
/// @{

/// how many elems the arr holds — moved only by trs_arr_copy_assign and trs_arr_swap
/// @param self the arr
/// @return the length
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_arr_len(const trs_Arr *self);

/// the size of one elem, as named at construction
/// @param self the arr
/// @return elem_size, which never moves
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_arr_elem_size(const trs_Arr *self);

/// the size of the block the arr owns
/// @param self the arr
/// @return len * elem_size
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_arr_bytes(const trs_Arr *self);

/// the allocator the arr was built with
/// @param self the arr
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] TRS_API
trs_Al *trs_arr_al(const trs_Arr *self);

/// @}

/// @name access
/// @{

/// the front elem
/// @param self asserts the arr is not empty
/// @return a pointer into the block, good until the arr is dropped, swapped or
///         copy-assigned into
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_arr_front(const trs_Arr *self);

/// the front elem, to write through
/// @copydetails trs_arr_front
[[nodiscard]] TRS_API
void *trs_arr_front_mut(trs_Arr *self);

/// the back elem
/// @copydetails trs_arr_front
[[nodiscard]] TRS_API
const void *trs_arr_back(const trs_Arr *self);

/// the back elem, to write through
/// @copydetails trs_arr_front
[[nodiscard]] TRS_API
void *trs_arr_back_mut(trs_Arr *self);

/// the elem at 'idx'
/// @param self the arr
/// @param idx asserts idx < len — out of range is a programmer error, not a status
/// @return a pointer into the block, good until the arr is dropped, swapped or
///         copy-assigned into
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_arr_get(const trs_Arr *self, size_t idx);

/// the elem at 'idx', to write through
/// @copydetails trs_arr_get
[[nodiscard]] TRS_API
void *trs_arr_get_mut(trs_Arr *self, size_t idx);

/// overwrites the elem at 'idx' with a copy of 'val'
/// @param self the arr
/// @param idx asserts idx < len
/// @param val the elem to copy in
/// @bigo{1}
TRS_API
void trs_arr_set(trs_Arr *self, size_t idx, const void *val);

/// the block itself
/// @param self the arr
/// @return the block, or null while the arr is empty; good until the arr is dropped,
///         swapped or copy-assigned into
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_arr_data(const trs_Arr *self);

/// the block itself, to write through
/// @copydetails trs_arr_data
[[nodiscard]] TRS_API
void *trs_arr_data_mut(trs_Arr *self);

/// @}

/// @name mods
/// @{

/// exchanges the two arrs whole, lengths and all
/// @param[in,out] self one arr
/// @param[in,out] other must have the same elem_size and the same allocator: the blocks
///                      change hands where they lie, so nothing is copied and nothing can
///                      fail. 'self' == 'other' is a no-op
/// @note two allocators are a broken precondition, not a runtime state — a block belongs to
///       the allocator that made it. To exchange across two, build each side on the other's
///       allocator with trs_arr_copy_with and hand the results over with
///       trs_arr_move_assign
/// @bigo{1}
TRS_API
void trs_arr_swap(trs_Arr *self, trs_Arr *other);

/// exchanges two elems in place
/// @param self the arr
/// @param i asserts i < len
/// @param j asserts j < len; i == j is a no-op
/// @bigo{1}
TRS_API
void trs_arr_swap_elems(trs_Arr *self, size_t i, size_t j);

/// @}

/// @name to span
/// @{

/// a writable view of the elems, the way in to algo
/// @param self the arr
/// @return a view good until the arr is dropped, swapped or copy-assigned into
/// @bigo{1}
[[nodiscard]] TRS_API
trs_SpanMut trs_arr_to_span_mut(trs_Arr *self);

/// a read-only view of the elems
/// @copydetails trs_arr_to_span_mut
[[nodiscard]] TRS_API
trs_Span trs_arr_to_span(const trs_Arr *self);

/// @}

/// @name print
/// @{

/// writes the elems to a stream as [a, b, c], followed by a newline
/// @param self the arr
/// @param stream where to write
/// @param fprint the printer, called once per elem
/// @bigo{n}
TRS_API
void trs_arr_fprint(const trs_Arr *self, FILE *stream, trs_FPrint fprint);

/// trs_arr_fprint to stdout
/// @param self the arr
/// @param fprint the printer, called once per elem
/// @bigo{n}
TRS_API
void trs_arr_print(const trs_Arr *self, trs_FPrint fprint);

/// @}

/// @name macros
/// @{

/// trs_arr_new_len with sizeof(T) for the elem size
/// @param T the elem type
/// @param len how many elems
/// @param al the allocator
/// @param[out] out where the new arr is written
/// @bigo{n}
#define TRS_ARR_NEW_LEN(T, len, al, out) \
    trs_arr_new_len((len), sizeof(T), (al), (out))

/// trs_arr_from_data with sizeof(T)
/// @param T the elem type
/// @param data the elems to copy in, made to typecheck as a const T *
/// @param len how many elems to read from 'data'
/// @param al the allocator
/// @param[out] out where the new arr is written
/// @bigo{n}
#define TRS_ARR_FROM_DATA(T, data, len, al, out) \
    trs_arr_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

/// a new arr from the elems written out: TRS_ARR_OF(int32_t, al, &a, 5, 3, 1)
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new arr is written
/// @param ... the elems, as a T initializer list
/// @bigo{n}
#define TRS_ARR_OF(T, al, out, ...)                     \
    trs_arr_from_data(                                  \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

/// trs_arr_front as a const T *
/// @param T the elem type
/// @param self the arr
/// @bigo{1}
#define TRS_ARR_FRONT_AS(T, self) \
    ((const T *) trs_arr_front((self)))

/// trs_arr_front_mut as a T *
/// @copydetails TRS_ARR_FRONT_AS
#define TRS_ARR_FRONT_MUT_AS(T, self) \
    ((T *) trs_arr_front_mut((self)))

/// trs_arr_back as a const T *
/// @copydetails TRS_ARR_FRONT_AS
#define TRS_ARR_BACK_AS(T, self) \
    ((const T *) trs_arr_back((self)))

/// trs_arr_back_mut as a T *
/// @copydetails TRS_ARR_FRONT_AS
#define TRS_ARR_BACK_MUT_AS(T, self) \
    ((T *) trs_arr_back_mut((self)))

/// trs_arr_get as a const T *
/// @param T the elem type
/// @param self the arr
/// @param idx the index
/// @bigo{1}
#define TRS_ARR_GET_AS(T, self, idx) \
    ((const T *) trs_arr_get((self), (idx)))

/// trs_arr_get_mut as a T *
/// @copydetails TRS_ARR_GET_AS
#define TRS_ARR_GET_MUT_AS(T, self, idx) \
    ((T *) trs_arr_get_mut((self), (idx)))

/// trs_arr_set from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the arr
/// @param idx the index
/// @param val the value to copy in
/// @bigo{1}
#define TRS_ARR_SET(T, self, idx, val) \
    trs_arr_set((self), (idx), &(T){ (val) })

/// @}

/// @}
