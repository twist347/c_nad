#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"

#include <stddef.h>

/// @file

/// @defgroup alloc_pool alloc/pool
/// @ingroup alloc
/// @brief an allocator that hands out blocks of one fixed size
///
/// One block, cut into equal pieces on a free list, so alloc and dealloc are both a
/// pointer move. That cost and the absence of fragmentation are what it buys; that every
/// request must fit one block is what it costs.
///
/// No realloc of its own, so nad_realloc falls back to alloc and copy — which fails as
/// soon as the new size is over a block. The parent is borrowed and has to outlive it.
///
/// @par Example
/// @snippet alloc/example_pool.c build
/// @snippet alloc/example_pool.c limits
/// @{

/// @name lifetime
/// @{

/// a pool of 'block_count' blocks of 'block_size' bytes, taken from 'parent'
/// @param parent where the block comes from, borrowed and not owned
/// @param block_size a floor, greater than 0: it is rounded up to the alignment and to
///                   what the free list needs, and nad_al_pool_stats reports the result
/// @param block_count how many blocks, greater than 0
/// @return the allocator, or null if the product overflowed or 'parent' had no block
/// @bigo{n} — the free list is threaded through every block
[[nodiscard]] NAD_API
nad_Al *nad_al_pool_new(nad_Al *parent, size_t block_size, size_t block_count);

/// gives the block back to the parent
/// @param al the pool; null is a no-op
/// @bigo{1}
NAD_API
void nad_al_pool_drop(nad_Al *al);

/// @}

/// @name mods
/// @{

/// takes every block back at once, leaving the pool as new
/// @param al the pool
/// @warning every pointer it ever handed out dies here
/// @bigo{n} — the free list is threaded again
NAD_API
void nad_al_pool_reset(nad_Al *al);

/// @}

/// @name stats
/// @{

/// What a pool is holding.
typedef struct {
    size_t block_size;   ///< the real size of one block, after the rounding up
    size_t block_count;  ///< how many there are in all
    size_t used;         ///< how many are handed out
    size_t free;         ///< how many are left
} nad_AlPoolStats;

/// what the pool is holding
/// @param al the pool
/// @return the four numbers
/// @bigo{1}
[[nodiscard]] NAD_API
nad_AlPoolStats nad_al_pool_stats(const nad_Al *al);

/// @}

/// @}
