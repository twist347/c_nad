#pragma once

#include "terse/alloc/alloc.h"
#include "terse/core/export.h"

#include <stddef.h>

/// @file

/// @defgroup alloc_pool alloc/pool
/// @ingroup alloc
/// @brief an allocator that hands out blocks of one fixed size
///
/// One block, cut into equal pieces on a free list, so alloc and dealloc are both a
/// pointer move and nothing fragments. The price is that every request must fit one
/// piece.
///
/// No realloc of its own, so trs_realloc falls back to alloc and copy — which fails as
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
///                   what the free list needs, and trs_al_pool_stats reports the result
/// @param block_count how many blocks, greater than 0
/// @return the allocator, or null if the product overflowed or 'parent' had no block
/// @bigo{n} — the free list is threaded through every block
[[nodiscard]] TRS_API
trs_Al *trs_al_pool_new(trs_Al *parent, size_t block_size, size_t block_count);

/// gives the block back to the parent
/// @param self the pool; null is a no-op
/// @bigo{1}
TRS_API
void trs_al_pool_drop(trs_Al *self);

/// @}

/// @name mods
/// @{

/// takes every block back at once, leaving the pool as new
/// @param self the pool
/// @warning every pointer it ever handed out dies here
/// @bigo{n} — the free list is threaded again
TRS_API
void trs_al_pool_reset(trs_Al *self);

/// @}

/// @name stats
/// @{

/// What a pool is holding.
typedef struct {
    size_t block_size;  ///< the real size of one block, after the rounding up
    size_t block_count; ///< how many there are in all
    size_t used;        ///< how many are handed out
    size_t free;        ///< how many are left
} trs_AlPoolStats;

/// what the pool is holding
/// @param self the pool
/// @return the four numbers
/// @bigo{1}
[[nodiscard]] TRS_API
trs_AlPoolStats trs_al_pool_stats(const trs_Al *self);

/// @}

/// @}
