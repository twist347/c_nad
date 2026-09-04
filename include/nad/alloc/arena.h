#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"

#include <stddef.h>

/// @file

/// @defgroup alloc_arena alloc/arena
/// @ingroup alloc
/// @brief an allocator that only ever bumps a pointer forward
///
/// One block, taken from the parent at construction and handed out in pieces. There is no
/// per-block free: dealloc is a no-op, and everything returns at once through
/// nad_al_arena_reset, which drops a whole phase of work in O(1).
///
/// No realloc of its own, so nad_realloc falls back to alloc and copy. The parent is
/// borrowed and has to outlive it.
///
/// @par Example
/// @snippet alloc/example_arena.c build
/// @snippet alloc/example_arena.c reset
/// @{

/// @name lifetime
/// @{

/// an arena over 'cap' bytes taken from 'parent'
/// @param parent where the block comes from, borrowed and not owned
/// @param cap how many bytes it will ever hand out, greater than 0
/// @return the allocator, or null if 'parent' could not give the block
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_al_arena_new(nad_Al *parent, size_t cap);

/// gives the block back to the parent
/// @param al the arena; null is a no-op
/// @bigo{1}
NAD_API
void nad_al_arena_drop(nad_Al *al);

/// @}

/// @name mods
/// @{

/// takes everything back at once, leaving the arena as new
/// @param al the arena
/// @warning every pointer it ever handed out dies here
/// @bigo{1}
NAD_API
void nad_al_arena_reset(nad_Al *al);

/// @}

/// @name stats
/// @{

/// What an arena is holding.
typedef struct {
    size_t cap;       ///< the block it was built with
    size_t used;      ///< how far the pointer is bumped, alignment padding and all
    size_t available; ///< cap - used, an upper bound on the next request
} nad_AlArenaStats;

/// what the arena is holding
/// @param al the arena
/// @return the three numbers
/// @bigo{1}
[[nodiscard]] NAD_API
nad_AlArenaStats nad_al_arena_stats(const nad_Al *al);

/// @}

/// @}
