#pragma once

#include "trs/alloc/alloc.h"
#include "trs/core/cmp.h"
#include "trs/core/hash.h"
#include "trs/core/status.h"
#include "trs/ds/hmap.h"

#include <stddef.h>

/*
 * The one door into ds/hmap that ds/hset needs and callers must not have.
 *
 * A set is a map whose values carry no information — Rust spells it HashMap<T, ()>. C has
 * no zero-sized type, so the zero has to be allowed somewhere, and here is the somewhere:
 * val_size may be 0 through this constructor and only through it. The public
 * trs_hmap_new_cap still refuses it, so no caller can build a map whose get returns a
 * pointer to nothing.
 *
 * What the zero buys is the whole reason it exists: with no value to follow the key there
 * is nothing to align it to, so the node is the header plus the key and not a byte more.
 */

/// like trs_hmap_new_cap, but 'val_size' may be 0. Not TRS_API: the library's default
/// hidden visibility keeps it out of the shared object's symbol table
[[nodiscard]]
trs_Status trs_hmap_new_raw_(
    size_t cap, size_t key_size, size_t val_size,
    trs_Hasher hasher, trs_Eq eq,
    trs_Al *al,
    trs_HMap **out
);
