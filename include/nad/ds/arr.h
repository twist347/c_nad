#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// @file

/// @defgroup ds_arr ds/arr
/// @ingroup ds
/// @brief nad_Arr — an owning array whose length is set when it is built
///
/// The elems live in one block, allocated once, and no operation changes how many there
/// are. This is ds/vec minus the growth.
///
/// The length moves only when the whole arr is replaced: nad_arr_copy_assign overwrites
/// one, nad_arr_swap exchanges two. Those are also the only two ops that move the block,
/// and so the only two that invalidate what nad_arr_get_mut, nad_arr_data and
/// nad_arr_to_span_mut handed out.
///
/// The view is writable, so algo sorts and fills in place through it. An index out of
/// range asserts; the ops that return a nad_Status are the ones that allocate.
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
/// An opaque handle: it comes from one of the constructors and goes back to nad_arr_drop
typedef struct nad_Arr nad_Arr;

/// @name lifetime
/// @{

/// a new arr of 'len' zeroed elems
/// @param len how many elems the arr will hold; 0 gives an arr that owns no block
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator, kept for everything after
/// @param[out] out the new arr, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n} — the block is zeroed
[[nodiscard]] NAD_API
nad_Status nad_arr_new_len(size_t len, size_t elem_size, nad_Al *al, nad_Arr **out);

/// a new arr holding a copy of 'len' elems read from 'data'
/// @param data the elems to copy in; may be null only when len is 0
/// @param len how many elems to read
/// @param elem_size the size of one elem, asserted greater than 0
/// @param al the allocator
/// @param[out] out the new arr, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated, or
///         len * elem_size overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_arr_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Arr **out);

/// a new arr holding a copy of what 's' views, taking its len and elem_size
/// @param s the view to copy
/// @param al the allocator; unrelated to where 's' points, the elems are copied out of it
/// @param[out] out the new arr, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_arr_from_span(nad_Span s, nad_Al *al, nad_Arr **out);

/// releases the block and the arr through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object; what the elems
///             point to is not released
/// @bigo{1}
NAD_API
void nad_arr_drop(nad_Arr *self);

/// @}

/// @name copy
/// @{

/// a new arr with the same elems, on the same allocator
/// @param self the arr to copy
/// @param[out] out the new arr, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_arr_copy(const nad_Arr *self, nad_Arr **out);

/// overwrites the elems of 'other' with those of 'self', resizing its block when the two
/// lengths differ
/// @param self the arr to copy from
/// @param[in,out] other must have the same elem_size; keeps its own allocator, and
///                      'self' == 'other' is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the block cannot be resized, leaving 'other' as
///         it was
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_arr_copy_assign(const nad_Arr *self, nad_Arr *other);

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
[[nodiscard]] NAD_API
bool nad_arr_eq(const nad_Arr *a, const nad_Arr *b);

/// whether the two hold equal elems under 'eq'
/// @param a one arr
/// @param b must have the same elem_size as 'a'
/// @param eq asked of every pair until one says no
/// @return whether the lengths match and every pair does
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_arr_eq_by(const nad_Arr *a, const nad_Arr *b, nad_Eq eq);

/// @}

/// @name info
/// @{

/// how many elems the arr holds — moved only by nad_arr_copy_assign and nad_arr_swap
/// @param self the arr
/// @return the length
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_arr_len(const nad_Arr *self);

/// the size of one elem, as named at construction
/// @param self the arr
/// @return elem_size, which never moves
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_arr_elem_size(const nad_Arr *self);

/// the size of the block the arr owns
/// @param self the arr
/// @return len * elem_size
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_arr_bytes(const nad_Arr *self);

/// the allocator the arr was built with
/// @param self the arr
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_arr_al(const nad_Arr *self);

/// @}

/// @name access
/// @{

/// the first elem
/// @param self asserts the arr is not empty
/// @return a pointer into the block, good until the arr is dropped, swapped or
///         copy-assigned into
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_arr_first(const nad_Arr *self);

/// the first elem, to write through
/// @copydetails nad_arr_first
[[nodiscard]] NAD_API
void *nad_arr_first_mut(nad_Arr *self);

/// the last elem
/// @copydetails nad_arr_first
[[nodiscard]] NAD_API
const void *nad_arr_last(const nad_Arr *self);

/// the last elem, to write through
/// @copydetails nad_arr_first
[[nodiscard]] NAD_API
void *nad_arr_last_mut(nad_Arr *self);

/// the elem at 'idx'
/// @param self the arr
/// @param idx asserts idx < len — out of range is a programmer error, not a status
/// @return a pointer into the block, good until the arr is dropped, swapped or
///         copy-assigned into
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_arr_get(const nad_Arr *self, size_t idx);

/// the elem at 'idx', to write through
/// @copydetails nad_arr_get
[[nodiscard]] NAD_API
void *nad_arr_get_mut(nad_Arr *self, size_t idx);

/// overwrites the elem at 'idx' with a copy of 'val'
/// @param self the arr
/// @param idx asserts idx < len
/// @param val the elem to copy in
/// @bigo{1}
NAD_API
void nad_arr_set(nad_Arr *self, size_t idx, const void *val);

/// the block itself
/// @param self the arr
/// @return the block, or null while the arr is empty; good until the arr is dropped,
///         swapped or copy-assigned into
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_arr_data(const nad_Arr *self);

/// the block itself, to write through
/// @copydetails nad_arr_data
[[nodiscard]] NAD_API
void *nad_arr_data_mut(nad_Arr *self);

/// @}

/// @name mods
/// @{

/// exchanges the two arrs whole, lengths and all
/// @param[in,out] self one arr
/// @param[in,out] other must have the same elem_size; 'self' == 'other' is a no-op
/// @retval NAD_STATUS_OK on success; on one allocator this swaps the two headers and
///         cannot fail
/// @retval NAD_STATUS_ERR_NO_MEM when the two sit on different allocators and the
///         elems cannot be moved, leaving both as they were
/// @bigo{1} on one allocator, n on two
[[nodiscard]] NAD_API
nad_Status nad_arr_swap(nad_Arr *self, nad_Arr *other);

/// exchanges two elems in place
/// @param self the arr
/// @param i asserts i < len
/// @param j asserts j < len; i == j is a no-op
/// @bigo{1}
NAD_API
void nad_arr_swap_elems(nad_Arr *self, size_t i, size_t j);

/// @}

/// @name to span
/// @{

/// a writable view of the elems, the way in to algo
/// @param self the arr
/// @return a view good until the arr is dropped, swapped or copy-assigned into
/// @bigo{1}
[[nodiscard]] NAD_API
nad_SpanMut nad_arr_to_span_mut(nad_Arr *self);

/// a read-only view of the elems
/// @copydetails nad_arr_to_span_mut
[[nodiscard]] NAD_API
nad_Span nad_arr_to_span(const nad_Arr *self);

/// @}

/// @name print
/// @{

/// writes the elems to a stream as [a, b, c], followed by a newline
/// @param self the arr
/// @param stream where to write
/// @param fprint the printer, called once per elem
/// @bigo{n}
NAD_API
void nad_arr_fprint(const nad_Arr *self, FILE *stream, nad_FPrint fprint);

/// nad_arr_fprint to stdout
/// @param self the arr
/// @param fprint the printer, called once per elem
/// @bigo{n}
NAD_API
void nad_arr_print(const nad_Arr *self, nad_FPrint fprint);

/// @}

/// @name macros
/// @{

/// nad_arr_new_len with sizeof(T) for the elem size
/// @param T the elem type
/// @param len how many elems
/// @param al the allocator
/// @param[out] out where the new arr is written
/// @bigo{n}
#define NAD_ARR_NEW_LEN(T, len, al, out) \
    nad_arr_new_len((len), sizeof(T), (al), (out))

/// nad_arr_from_data with sizeof(T)
/// @param T the elem type
/// @param data the elems to copy in, made to typecheck as a const T *
/// @param len how many elems to read from 'data'
/// @param al the allocator
/// @param[out] out where the new arr is written
/// @bigo{n}
#define NAD_ARR_FROM_DATA(T, data, len, al, out) \
    nad_arr_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

/// a new arr from the elems written out: NAD_ARR_OF(int32_t, al, &a, 5, 3, 1)
/// @param T the elem type
/// @param al the allocator
/// @param[out] out where the new arr is written
/// @param ... the elems, as a T initializer list
/// @bigo{n}
#define NAD_ARR_OF(T, al, out, ...)                     \
    nad_arr_from_data(                                  \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

/// nad_arr_first as a const T *
/// @param T the elem type
/// @param self the arr
/// @bigo{1}
#define NAD_ARR_FIRST_AS(T, self) \
    ((const T *) nad_arr_first((self)))

/// nad_arr_first_mut as a T *
/// @copydetails NAD_ARR_FIRST_AS
#define NAD_ARR_FIRST_MUT_AS(T, self) \
    ((T *) nad_arr_first_mut((self)))

/// nad_arr_last as a const T *
/// @copydetails NAD_ARR_FIRST_AS
#define NAD_ARR_LAST_AS(T, self) \
    ((const T *) nad_arr_last((self)))

/// nad_arr_last_mut as a T *
/// @copydetails NAD_ARR_FIRST_AS
#define NAD_ARR_LAST_MUT_AS(T, self) \
    ((T *) nad_arr_last_mut((self)))

/// nad_arr_get as a const T *
/// @param T the elem type
/// @param self the arr
/// @param idx the index
/// @bigo{1}
#define NAD_ARR_GET_AS(T, self, idx) \
    ((const T *) nad_arr_get((self), (idx)))

/// nad_arr_get_mut as a T *
/// @copydetails NAD_ARR_GET_AS
#define NAD_ARR_GET_MUT_AS(T, self, idx) \
    ((T *) nad_arr_get_mut((self), (idx)))

/// nad_arr_set from a value rather than an address
/// @param T the elem type; a scalar, since 'val' becomes a compound literal
/// @param self the arr
/// @param idx the index
/// @param val the value to copy in
/// @bigo{1}
#define NAD_ARR_SET(T, self, idx, val) \
    nad_arr_set((self), (idx), &(T){ (val) })

/// @}

/// @}
