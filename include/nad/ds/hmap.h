#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/hash.h"
#include "nad/core/print.h"
#include "nad/core/status.h"

#include <stddef.h>

/// owning hash map from type-erased keys to type-erased values, built on separate
/// chaining: an array of buckets, each holding a chain of nodes that hashed to it.
///
/// The hasher and the equality are properties of the map, not of a call: they are fixed
/// at construction and travel with the entries through copy and swap, since a bucket
/// means nothing without the hash that chose it. They must agree — keys equal under 'eq'
/// must hash alike, which is the contract nad_Hasher states.
///
/// An entry never moves. Growing reallocates the bucket array and RELINKS the nodes into
/// it; the nodes themselves stay where they were, so a borrowed node survives every
/// operation but the removal of that very entry. That is what chaining buys over the flat
/// alternative, and it is why a position here is a node rather than a slot index.
///
/// The iteration order is unspecified and may change on any insert that grows: it follows
/// the buckets, and which bucket a key lands in is the hash's business.
typedef struct nad_HMap nad_HMap;

/// a position in the map; borrowed from it and invalidated only by removing that very
/// entry. Its key is readable but never writable — a written key would belong to a bucket
/// the map no longer chose for it, with no way for the map to notice
typedef struct nad_HMapNode nad_HMapNode;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Status nad_hmap_new(size_t key_size, size_t val_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al, nad_HMap **out);

/// room for 'cap' entries before the first growth
[[nodiscard]] NAD_API
nad_Status nad_hmap_new_cap(size_t cap, size_t key_size, size_t val_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al,
                            nad_HMap **out);

NAD_API
void nad_hmap_drop(nad_HMap *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API
nad_Status nad_hmap_copy(const nad_HMap *self, nad_HMap **out);

/// 'other' receives the hasher and the equality along with the entries, overwriting its
/// own — as in pqueue, where the comparator travels with the elems
[[nodiscard]] NAD_API
nad_Status nad_hmap_copy_assign(const nad_HMap *self, nad_HMap *other);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_hmap_len(const nad_HMap *self);

/// how many buckets the entries are spread over; always a power of two, and 0 while the
/// map has never held anything
[[nodiscard]] NAD_API
size_t nad_hmap_bucket_count(const nad_HMap *self);

[[nodiscard]] NAD_API
size_t nad_hmap_key_size(const nad_HMap *self);

[[nodiscard]] NAD_API
size_t nad_hmap_val_size(const nad_HMap *self);

[[nodiscard]] NAD_API
nad_Al *nad_hmap_al(const nad_HMap *self);

[[nodiscard]] NAD_API
nad_Hasher nad_hmap_hasher(const nad_HMap *self);

[[nodiscard]] NAD_API
nad_Eq nad_hmap_eq(const nad_HMap *self);

/* ========== lookup ========== */

/// the value stored under 'key', or null when the map holds no such key. A miss is not an
/// error and has exactly one cause, so it needs no status of its own — the same reading
/// nad_span_find takes of "not found"
[[nodiscard]] NAD_API
const void *nad_hmap_get(const nad_HMap *self, const void *key);

[[nodiscard]] NAD_API
void *nad_hmap_get_mut(nad_HMap *self, const void *key);

[[nodiscard]] NAD_API
bool nad_hmap_contains(const nad_HMap *self, const void *key);

/// the entry under 'key' as a position, or null when there is none. Hold it to reach the
/// key and the value together, or to remove the entry without hashing it again
[[nodiscard]] NAD_API
const nad_HMapNode *nad_hmap_find(const nad_HMap *self, const void *key);

[[nodiscard]] NAD_API
nad_HMapNode *nad_hmap_find_mut(nad_HMap *self, const void *key);

/* ========== nodes ========== */

/// the first entry in bucket order, or null on an empty map
[[nodiscard]] NAD_API
const nad_HMapNode *nad_hmap_first_node(const nad_HMap *self);

[[nodiscard]] NAD_API
nad_HMapNode *nad_hmap_first_node_mut(nad_HMap *self);

/// the entry after 'node', or null at the end. The map is needed because a chain ends
/// long before the buckets do — unlike a list node, which knows its own successor
[[nodiscard]] NAD_API
const nad_HMapNode *nad_hmap_node_next(const nad_HMap *self, const nad_HMapNode *node);

[[nodiscard]] NAD_API
nad_HMapNode *nad_hmap_node_next_mut(nad_HMap *self, nad_HMapNode *node);

/// the key sits at the front of the node, so this one needs nothing else
[[nodiscard]] NAD_API
const void *nad_hmap_node_key(const nad_HMapNode *node);

/// the map is needed again here, for a different reason than in node_next: the value sits
/// behind the key, and how far behind is a property of the map's key size rather than of
/// the node
[[nodiscard]] NAD_API
const void *nad_hmap_node_val(const nad_HMap *self, const nad_HMapNode *node);

/// there is no node_key_mut to match this: the value is the map's to hand over, the key
/// is not
[[nodiscard]] NAD_API
void *nad_hmap_node_val_mut(const nad_HMap *self, nad_HMapNode *node);

/* ========== mods ========== */

/// stores 'val' under 'key', overwriting the value already there if the key is present.
/// 'out_is_new' says whether the key was absent and may be null when the caller does not
/// care — it reports an observation rather than the operation's result, which is the map
/// itself. Written only on NAD_STATUS_OK, as every out is
[[nodiscard]] NAD_API
nad_Status nad_hmap_insert(nad_HMap *self, const void *key, const void *val, bool *out_is_new);

/// drops the entry under 'key' and says whether there was one. Nothing is allocated, so
/// nothing can fail
NAD_API
bool nad_hmap_remove(nad_HMap *self, const void *key);

/// drops the entry 'node' names, which must belong to this map. The bucket is found from
/// the hash the node carries, so the key is never hashed again
NAD_API
void nad_hmap_remove_node(nad_HMap *self, nad_HMapNode *node);

/// hands back the entry for 'key', putting 'val_if_absent' there first when the key was
/// missing. One hash and one walk of the bucket either way, which is what a lookup
/// followed by an insert cannot be: that pair hashes twice and walks twice whenever the
/// key turns out to be absent. This is the counter idiom's operation
[[nodiscard]] NAD_API
nad_Status nad_hmap_get_or_insert(nad_HMap *self, const void *key, const void *val_if_absent,
                                  nad_HMapNode **out_node);

/// drops every entry and keeps the buckets
NAD_API
void nad_hmap_clear(nad_HMap *self);

/// room for 'cap' entries without a growth in between
[[nodiscard]] NAD_API
nad_Status nad_hmap_reserve(nad_HMap *self, size_t cap);

/// gives back the buckets the entries no longer need, down to the smallest power of two
/// that still holds them and never below the count a fresh map takes; an empty map is
/// left owning nothing at all. Only the bucket
/// array is reallocated — the entries are relinked where they lie, so a borrowed node
/// survives this as it survives a growth
[[nodiscard]] NAD_API
nad_Status nad_hmap_shrink_to_fit(nad_HMap *self);

/// both maps must share an allocator: the nodes change map without moving, so every
/// borrowed node stays valid — the same trade ds/list makes. The hasher and the equality
/// change sides with the entries, so two maps built under different ones stay valid maps
NAD_API
void nad_hmap_swap(nad_HMap *self, nad_HMap *other);

/* ========== print ========== */

NAD_API
void nad_hmap_fprint(const nad_HMap *self, FILE *stream, nad_FPrint key_fprint, nad_FPrint val_fprint);

NAD_API
void nad_hmap_print(const nad_HMap *self, nad_FPrint key_fprint, nad_FPrint val_fprint);

/* ========== macros ========== */

#define NAD_HMAP_NEW(K, V, hasher, eq, al, out) \
    nad_hmap_new(sizeof(K), sizeof(V), (hasher), (eq), (al), (out))

#define NAD_HMAP_NEW_CAP(K, V, cap, hasher, eq, al, out) \
    nad_hmap_new_cap((cap), sizeof(K), sizeof(V), (hasher), (eq), (al), (out))

#define NAD_HMAP_GET_AS(K, V, self, key) \
    ((const V *) nad_hmap_get((self), &(K){ (key) }))

#define NAD_HMAP_GET_MUT_AS(K, V, self, key) \
    ((V *) nad_hmap_get_mut((self), &(K){ (key) }))

#define NAD_HMAP_CONTAINS(K, self, key) \
    nad_hmap_contains((self), &(K){ (key) })

#define NAD_HMAP_FIND(K, self, key) \
    nad_hmap_find((self), &(K){ (key) })

#define NAD_HMAP_INSERT(K, V, self, key, val, out_is_new) \
    nad_hmap_insert((self), &(K){ (key) }, &(V){ (val) }, (out_is_new))

#define NAD_HMAP_GET_OR_INSERT(K, V, self, key, val, out_node) \
    nad_hmap_get_or_insert((self), &(K){ (key) }, &(V){ (val) }, (out_node))

#define NAD_HMAP_REMOVE(K, self, key) \
    nad_hmap_remove((self), &(K){ (key) })

#define NAD_HMAP_NODE_KEY_AS(K, node) \
    ((const K *) nad_hmap_node_key((node)))

#define NAD_HMAP_NODE_VAL_AS(V, self, node) \
    ((const V *) nad_hmap_node_val((self), (node)))

#define NAD_HMAP_NODE_VAL_MUT_AS(V, self, node) \
    ((V *) nad_hmap_node_val_mut((self), (node)))
