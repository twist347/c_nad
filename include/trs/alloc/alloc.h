#pragma once

#include "trs/core/export.h"

#include <stddef.h>
#include <stdint.h>

/// @file

/// @defgroup alloc_alloc alloc/alloc
/// @ingroup alloc
/// @brief trs_Al — the allocator everything that owns memory is built on
///
/// An interface you dispatch through, not an object you own, so the operations are named
/// for the verb and take the allocator first: trs_alloc(al, n), the way fprintf takes its
/// FILE. Only ops about the allocator itself carry a slug, as trs_al_default does.
///
/// The wrappers return the pointer and say failure with null — the one place a fallible
/// op returns no trs_Status, since [[nodiscard]] on the pointer flags the same dropped
/// check.
/// Null is not always failure: asking for nothing gives nothing, and each wrapper says
/// which of its nulls mean what.
///
/// A table must fill in alloc and dealloc and may leave calloc and realloc null — the
/// wrapper builds those out of the other two, which is how the arena and the pool live.
/// An allocator over another borrows it: the parent outlives the child, and the two drop
/// in the reverse order of building.
///
/// @par Example
/// @snippet alloc/example_alloc.c use
/// @snippet alloc/example_alloc.c custom
/// @snippet alloc/example_alloc.c wrap
/// @{

/// Four operations and the state they run on, passed around by pointer.
typedef struct {
    void *ctx; ///< whatever the implementation keeps; handed back to every callback

    /// 'size' bytes, uninitialized, or null. Required.
    void *(*alloc)(void *ctx, size_t size);

    /// 'num' * 'size' zeroed bytes, or null. Optional: null here means alloc and memset.
    void *(*calloc)(void *ctx, size_t num, size_t size);

    /// 'ptr' moved from 'old_size' to 'new_size' bytes, or null. Optional: null here
    /// means alloc, copy, dealloc.
    void *(*realloc)(void *ctx, void *ptr, size_t old_size, size_t new_size);

    /// gives back 'size' bytes at 'ptr'. Required.
    void (*dealloc)(void *ctx, void *ptr, size_t size);
} trs_Al;

/// @name wrappers
/// @{

/// 'size' bytes, uninitialized
/// @param al the allocator
/// @param size how many bytes; 0 asks for nothing and gives null, which is defined
/// @return the block, or null — with 'size' over 0, null is out of memory and nothing else
[[nodiscard]] TRS_API
void *trs_alloc(trs_Al *al, size_t size);

/// 'num' * 'size' bytes, zeroed
/// @param al the allocator
/// @param num how many elems
/// @param size the size of one
/// @return the block, or null — either operand was 0, the product overflowed size_t, or
///         the allocation failed; on a non-zero, non-overflowing request only the last
[[nodiscard]] TRS_API
void *trs_calloc(trs_Al *al, size_t num, size_t size);

/// the block at 'ptr', resized to 'new_size' bytes
/// @param al the allocator
/// @param ptr the block to resize; may be null only when 'old_size' is 0
/// @param old_size what 'ptr' was allocated as
/// @param new_size what it should become; 0 gives 'ptr' back and returns null, defined
///                 and not a failure
/// @return the block, which may have moved, or null on failure with 'ptr' untouched
/// @warning assign to a temporary, never over 'ptr' — on failure the old block is still
///          yours, and overwriting its only pointer leaks it
[[nodiscard]] TRS_API
void *trs_realloc(trs_Al *al, void *ptr, size_t old_size, size_t new_size);

/// gives the block back
/// @param al the allocator; must be the one 'ptr' came from
/// @param ptr the block; null is a no-op
/// @param size what 'ptr' was allocated as; it must match, an allocator may need it
TRS_API
void trs_dealloc(trs_Al *al, void *ptr, size_t size);

/// @}

/// @name macros
/// @{

/// trs_alloc for 'count' elems of T, with the multiplication checked
/// @param T the elem type
/// @param al the allocator
/// @param count how many elems; one that would overflow gives null, not a wrapped request
#define TRS_ALLOC(T, al, count)                \
    ((T *) ((count) > SIZE_MAX / sizeof(T)     \
        ? nullptr                              \
        : trs_alloc((al), (count) * sizeof(T))))

/// trs_calloc for 'count' elems of T — the overflow check is trs_calloc's own
/// @param T the elem type
/// @param al the allocator
/// @param count how many elems
#define TRS_CALLOC(T, al, count) \
    ((T*) trs_calloc((al), (count), sizeof(T)))

/// trs_realloc between two counts of T, with the multiplication checked
/// @param T the elem type
/// @param al the allocator
/// @param ptr the block to resize
/// @param old_count what it holds now
/// @param new_count what it should hold; one that would overflow gives null
#define TRS_REALLOC(T, al, ptr, old_count, new_count)                                 \
    ((T *) ((new_count) > SIZE_MAX / sizeof(T)                                        \
        ? nullptr                                                                     \
        : trs_realloc((al), (ptr), (old_count) * sizeof(T), (new_count) * sizeof(T))))

/// trs_dealloc for 'count' elems of T
/// @param T the elem type
/// @param al the allocator
/// @param ptr the block
/// @param count what it holds
#define TRS_DEALLOC(T, al, ptr, count) \
    trs_dealloc((al), (ptr), (count) * sizeof(T))

/// @}

/// @}
