#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"

#include <stddef.h>

/// @file

/// @defgroup alloc_aligned alloc/aligned
/// @ingroup alloc
/// @brief an allocator whose every block is over-aligned
///
/// nad_Al has four operations and no alignment to pass, so alignment is a property of the
/// allocator and not of the call: a type that needs more than max_align_t gets it by being
/// handed this allocator, and the container over it keeps its own code.
///
/// A request goes to the parent 'alignment' - 1 + sizeof(void *) bytes larger, the block
/// is walked forward to the next aligned address, and the address the parent gave is kept
/// in the word before it.
///
/// No calloc and no realloc of its own: the wrappers build both out of this alloc, which
/// is what keeps the result aligned — the parent's realloc would not. The parent is
/// borrowed and has to outlive it.
///
/// @par Example
/// @snippet alloc/example_aligned.c build
/// @{

/// @name lifetime
/// @{

/// an allocator handing out blocks aligned to 'alignment', taken from 'parent'
/// @param parent where the memory comes from, borrowed and not owned
/// @param alignment a power of two, at least alignof(max_align_t) — under that the parent
///                  already delivers and the wrapper is pure overhead
/// @return the allocator, or null if 'parent' could not give the two blocks it needs
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_al_aligned_new(nad_Al *parent, size_t alignment);

/// gives its own two blocks back to the parent
/// @param al the allocator; null is a no-op
/// @warning it frees nothing it handed out — every block goes back through nad_dealloc
///          first, as with nad_al_default
/// @bigo{1}
NAD_API
void nad_al_aligned_drop(nad_Al *al);

/// @}

/// @}
