#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/hash.h"
#include "nad/core/print.h"
#include "nad/core/status.h"
#include "nad/ds/hmap.h"

#include <stddef.h>

/// owning hash set of type-erased keys: a ds/hmap whose values carry no information.
/// Rust spells the same thought HashMap<T, ()>; C has no zero-sized type, so the zero
/// lives one level down, where a set's node comes out as the header plus the key and not
/// a byte more.
///
/// The hasher and the equality are fixed at construction and travel with the keys through
/// copy and swap, as in ds/hmap. They must agree: keys equal under 'eq' must hash alike.
///
/// What this adds over the map it wraps is a NARROWER interface, not an invariant over the
/// keys: there is no value to get, to set or to hand out. An entry is either in or out.
///
/// A key never moves. Growing reallocates the bucket array and relinks the nodes, so a
/// borrowed node survives everything but the removal of that very key.
typedef struct nad_HSet nad_HSet;

/// a position in the set. A set entry IS a map entry — the same node, seen through an API
/// that offers no value side — so the two spell the position with one type
typedef nad_HMapNode nad_HSetNode;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Status nad_hset_new(size_t key_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al, nad_HSet **out);

/// room for 'cap' keys before the first growth
[[nodiscard]] NAD_API
nad_Status nad_hset_new_cap(size_t cap, size_t key_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al, nad_HSet **out);

NAD_API
void nad_hset_drop(nad_HSet *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API
nad_Status nad_hset_copy(const nad_HSet *self, nad_HSet **out);

/// 'other' receives the hasher and the equality along with the keys, overwriting its own
[[nodiscard]] NAD_API
nad_Status nad_hset_copy_assign(const nad_HSet *self, nad_HSet *other);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_hset_len(const nad_HSet *self);

[[nodiscard]] NAD_API
size_t nad_hset_bucket_count(const nad_HSet *self);

[[nodiscard]] NAD_API
size_t nad_hset_key_size(const nad_HSet *self);

[[nodiscard]] NAD_API
nad_Al *nad_hset_al(const nad_HSet *self);

[[nodiscard]] NAD_API
nad_Hasher nad_hset_hasher(const nad_HSet *self);

[[nodiscard]] NAD_API
nad_Eq nad_hset_eq(const nad_HSet *self);

/* ========== lookup ========== */

[[nodiscard]] NAD_API
bool nad_hset_contains(const nad_HSet *self, const void *key);

/// the entry for 'key' as a position, or null when the set holds no such key
[[nodiscard]] NAD_API
const nad_HSetNode *nad_hset_find(const nad_HSet *self, const void *key);

[[nodiscard]] NAD_API
nad_HSetNode *nad_hset_find_mut(nad_HSet *self, const void *key);

/* ========== nodes ========== */

[[nodiscard]] NAD_API
const nad_HSetNode *nad_hset_first_node(const nad_HSet *self);

[[nodiscard]] NAD_API
nad_HSetNode *nad_hset_first_node_mut(nad_HSet *self);

/// the entry after 'node', or null at the end. The set is needed because a chain ends long
/// before the buckets do
[[nodiscard]] NAD_API
const nad_HSetNode *nad_hset_node_next(const nad_HSet *self, const nad_HSetNode *node);

[[nodiscard]] NAD_API
nad_HSetNode *nad_hset_node_next_mut(nad_HSet *self, nad_HSetNode *node);

/// read only, and there is nothing beside it: a written key would belong to a bucket the
/// set no longer chose for it
[[nodiscard]] NAD_API
const void *nad_hset_node_key(const nad_HSetNode *node);

/* ========== mods ========== */

/// puts 'key' in the set. 'out_is_new' says whether it was absent and may be null when the
/// caller does not care; written only on NAD_STATUS_OK
[[nodiscard]] NAD_API
nad_Status nad_hset_insert(nad_HSet *self, const void *key, bool *out_is_new);

/// drops 'key' and says whether it was there. Nothing is allocated, so nothing can fail
NAD_API
bool nad_hset_remove(nad_HSet *self, const void *key);

/// drops the entry 'node' names, which must belong to this set
NAD_API
void nad_hset_remove_node(nad_HSet *self, nad_HSetNode *node);

NAD_API
void nad_hset_clear(nad_HSet *self);

[[nodiscard]] NAD_API
nad_Status nad_hset_reserve(nad_HSet *self, size_t cap);

/// gives back the buckets the keys no longer need; an empty set is left owning nothing.
/// The nodes are relinked, not rebuilt, so a borrowed one survives this
[[nodiscard]] NAD_API
nad_Status nad_hset_shrink_to_fit(nad_HSet *self);

/// both sets must share an allocator: the nodes change set without moving, so every
/// borrowed node stays valid — the same trade ds/hmap and ds/list make
NAD_API
void nad_hset_swap(nad_HSet *self, nad_HSet *other);

/* ========== print ========== */

NAD_API
void nad_hset_fprint(const nad_HSet *self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_hset_print(const nad_HSet *self, nad_FPrint fprint);

/* ========== macros ========== */

#define NAD_HSET_NEW(K, hasher, eq, al, out) \
    nad_hset_new(sizeof(K), (hasher), (eq), (al), (out))

#define NAD_HSET_NEW_CAP(K, cap, hasher, eq, al, out) \
    nad_hset_new_cap((cap), sizeof(K), (hasher), (eq), (al), (out))

#define NAD_HSET_INSERT(K, self, key, out_is_new) \
    nad_hset_insert((self), &(K){ (key) }, (out_is_new))

#define NAD_HSET_CONTAINS(K, self, key) \
    nad_hset_contains((self), &(K){ (key) })

#define NAD_HSET_FIND(K, self, key) \
    nad_hset_find((self), &(K){ (key) })

#define NAD_HSET_REMOVE(K, self, key) \
    nad_hset_remove((self), &(K){ (key) })

#define NAD_HSET_NODE_KEY_AS(K, node) \
    ((const K *) nad_hset_node_key((node)))
