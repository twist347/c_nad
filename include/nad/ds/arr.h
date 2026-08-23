#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// @file

/// @defgroup ds_arr ds/arr
/// @ingroup ds
/// @brief nad_Arr — an owning array whose length is fixed when it is built
///
/// The elems live in one block, allocated once, and the length is fixed at construction.
///
/// This is ds/vec minus the growth: no capacity, no operation that changes a length.
/// Because the block never moves, pointers from
/// nad_arr_get_mut, views from nad_arr_to_span_mut and nad_arr_data stay good for the
/// life of the arr; only nad_arr_swap breaks that, by exchanging two arrs whole.
///
/// The view is writable, so algo sorts, partitions and fills in place through it — an arr
/// keeps no order of its own to protect, unlike ds/stack and ds/queue. An index out of
/// range asserts; the ops that return a nad_Status are the ones that allocate.
/// @{

/// owning array whose length is fixed when it is built.
/// An opaque handle: it comes from one of the constructors and goes back to nad_arr_drop
typedef struct nad_Arr nad_Arr;

/* ========== lifetime ========== */

/// a new arr of 'len' zeroed elems
/// @param len how many elems the arr will hold; 0 gives an arr that owns no block
/// @param elem_size the size of one elem in bytes, greater than 0
/// @param al the allocator to build on, kept and used for everything after
/// @param[out] out the new arr, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_OUT_OF_MEMORY when the header or the block cannot be allocated,
///         or when len * elem_size overflows
/// @bigo{n} — the block is zeroed
[[nodiscard]] NAD_API
nad_Status nad_arr_new_len(size_t len, size_t elem_size, nad_Al *al, nad_Arr **out);

/// a new arr holding a copy of the elems read from 'data'
/// @param data the elems to copy in; may be null only when 'len' is 0
/// @param len how many elems to read from 'data'
/// @param elem_size the size of one elem in bytes, greater than 0
/// @param al the allocator to build on, kept and used for everything after
/// @param[out] out the new arr, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_OUT_OF_MEMORY when the header or the block cannot be allocated,
///         or when len * elem_size overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_arr_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Arr **out);

/// a new arr holding a copy of what 's' views
/// @param s the elems to copy in; it also names the len and the elem_size
/// @param al the allocator to build on — it has nothing to do with where 's' points,
///           the elems are copied out of it
/// @param[out] out the new arr, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_OUT_OF_MEMORY when the header or the block cannot be allocated,
///         or when len * elem_size overflows
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_arr_from_span(nad_Span s, nad_Al *al, nad_Arr **out);

/// releases the block and the arr itself through the allocator it was built with
/// @param self the arr; null is a no-op, so this can be called on a partly built object
/// @bigo{1}
NAD_API
void nad_arr_drop(nad_Arr *self);

/* ========== copy ========== */

/// a new arr with the same elems, built on the same allocator as 'self'
/// @param self the arr to copy
/// @param[out] out the new arr, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_OUT_OF_MEMORY when the header or the block cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_arr_copy(const nad_Arr *self, nad_Arr **out);

/// overwrites the elems of 'other' with those of 'self', resizing its block when the two
/// lengths differ
/// @param self the arr to copy from
/// @param[in,out] other the arr written into; must have the same elem_size as 'self',
///                      and 'self' == 'other' is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_OUT_OF_MEMORY when the block of 'other' cannot be resized; on
///         failure 'other' is left exactly as it was
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_arr_copy_assign(const nad_Arr *self, nad_Arr *other);

/* ========== info ========== */

/// how many elems the arr holds — fixed for its whole life
/// @param self the arr
/// @return the length
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_arr_len(const nad_Arr *self);

/// the size of one elem in bytes, as named at construction
/// @param self the arr
/// @return the elem size
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_arr_elem_size(const nad_Arr *self);

/// the size of the block the arr owns
/// @param self the arr
/// @return len * elem_size
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_arr_bytes(const nad_Arr *self);

/// the allocator the arr was built with and uses for everything
/// @param self the arr
/// @return the allocator, borrowed — the arr does not own it
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_arr_al(const nad_Arr *self);

/* ========== access ========== */

/// the first elem
/// @param self the arr; asserts it is not empty
/// @return the elem at 0
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_arr_first(const nad_Arr *self);

/// the first elem, to write through
/// @param self the arr; asserts it is not empty
/// @return the elem at 0
/// @bigo{1}
[[nodiscard]] NAD_API
void *nad_arr_first_mut(nad_Arr *self);

/// the last elem
/// @param self the arr; asserts it is not empty
/// @return the elem at len - 1
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_arr_last(const nad_Arr *self);

/// the last elem, to write through
/// @param self the arr; asserts it is not empty
/// @return the elem at len - 1
/// @bigo{1}
[[nodiscard]] NAD_API
void *nad_arr_last_mut(nad_Arr *self);

/// the elem at 'idx'
/// @param self the arr
/// @param idx the index; asserts idx < len — out of range is a programmer error, not a
///            status
/// @return the elem
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_arr_get(const nad_Arr *self, size_t idx);

/// the elem at 'idx', to write through
/// @param self the arr
/// @param idx the index; asserts idx < len
/// @return the elem
/// @bigo{1}
[[nodiscard]] NAD_API
void *nad_arr_get_mut(nad_Arr *self, size_t idx);

/// writes one elem over the elem at 'idx'
/// @param self the arr
/// @param idx the index; asserts idx < len
/// @param val the address of the value to copy in — elem_size bytes are read from it
/// @bigo{1}
NAD_API
void nad_arr_set(nad_Arr *self, size_t idx, const void *val);

/// the block itself, for handing to code that wants a plain pointer
/// @param self the arr
/// @return the block, or null while the arr is empty
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_arr_data(const nad_Arr *self);

/// the block itself, to write through
/// @param self the arr
/// @return the block, or null while the arr is empty
/// @bigo{1}
[[nodiscard]] NAD_API
void *nad_arr_data_mut(nad_Arr *self);

/* ========== mods ========== */

/// exchanges the two arrs whole, lengths and all
/// @param[in,out] self one arr
/// @param[in,out] other the other; must have the same elem_size, and 'self' == 'other'
///                      is a no-op
/// @retval NAD_STATUS_OK on success — on one allocator this only swaps the two headers
///         and cannot fail
/// @retval NAD_STATUS_OUT_OF_MEMORY when the two are on different allocators and the
///         elems cannot be moved; on failure neither arr is touched
/// @bigo{1} on one allocator, O(n) across two
[[nodiscard]] NAD_API
nad_Status nad_arr_swap(nad_Arr *self, nad_Arr *other);

/// exchanges two elems in place
/// @param self the arr
/// @param i one index; asserts it is in range
/// @param j the other index; asserts it is in range
/// @bigo{1}
NAD_API
void nad_arr_swap_elems(nad_Arr *self, size_t i, size_t j);

/* ========== to span ========== */

/// a writable view over the elems: the bridge to algo, which sorts, partitions and fills
/// through it
/// @param self the arr
/// @return the view, good until the arr is dropped or swapped
/// @bigo{1}
[[nodiscard]] NAD_API
nad_SpanMut nad_arr_to_span_mut(nad_Arr *self);

/// a read-only view over the elems
/// @param self the arr
/// @return the view, good until the arr is dropped or swapped
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Span nad_arr_to_span(const nad_Arr *self);

/* ========== print ========== */

/// writes the elems to a stream
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

/* ========== macros ========== */

/// nad_arr_new_len with sizeof(T) for the elem size
/// @param T the elem type
/// @param len how many elems
/// @param al the allocator
/// @param out where the new arr is written
#define NAD_ARR_NEW_LEN(T, len, al, out) \
    nad_arr_new_len((len), sizeof(T), (al), (out))

/// nad_arr_from_data with sizeof(T)
/// @param T the elem type
/// @param data the elems to copy in, made to typecheck as a const T *
/// @param len how many elems to read from 'data'
/// @param al the allocator
/// @param out where the new arr is written
#define NAD_ARR_FROM_DATA(T, data, len, al, out) \
    nad_arr_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

/// a new arr from the elems written out: NAD_ARR_OF(int32_t, al, &a, 5, 3, 1)
/// @param T the elem type
/// @param al the allocator
/// @param out where the new arr is written
/// @param ... the elems, as a T initializer list
#define NAD_ARR_OF(T, al, out, ...)                     \
    nad_arr_from_data(                                  \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

/// nad_arr_first as a const T *
/// @param T the elem type
/// @param self the arr
#define NAD_ARR_FIRST_AS(T, self) \
    ((const T *) nad_arr_first((self)))

/// nad_arr_first_mut as a T *
/// @param T the elem type
/// @param self the arr
#define NAD_ARR_FIRST_MUT_AS(T, self) \
    ((T *) nad_arr_first_mut((self)))

/// nad_arr_last as a const T *
/// @param T the elem type
/// @param self the arr
#define NAD_ARR_LAST_AS(T, self) \
    ((const T *) nad_arr_last((self)))

/// nad_arr_last_mut as a T *
/// @param T the elem type
/// @param self the arr
#define NAD_ARR_LAST_MUT_AS(T, self) \
    ((T *) nad_arr_last_mut((self)))

/// nad_arr_get as a const T *
/// @param T the elem type
/// @param self the arr
/// @param idx the index
#define NAD_ARR_GET_AS(T, self, idx) \
    ((const T *) nad_arr_get((self), (idx)))

/// nad_arr_get_mut as a T *
/// @param T the elem type
/// @param self the arr
/// @param idx the index
#define NAD_ARR_GET_MUT_AS(T, self, idx) \
    ((T *) nad_arr_get_mut((self), (idx)))

/// nad_arr_set from a value rather than its address
/// @param T the elem type — scalars only, a struct or an array cannot be written as
///          (T){ val }
/// @param self the arr
/// @param idx the index
/// @param val the value to copy in
#define NAD_ARR_SET(T, self, idx, val) \
    nad_arr_set((self), (idx), &(T){ (val) })

/// @}
