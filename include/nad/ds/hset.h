#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/hash.h"
#include "nad/core/print.h"
#include "nad/core/status.h"
#include "nad/ds/hmap.h"

#include <stddef.h>

/// @file

/// @defgroup ds_hset ds/hset
/// @ingroup ds
/// @brief nad_HSet — an owning hash set of type-erased keys
///
/// A ds/hmap whose values carry no information. C has no zero-sized type, so the zero
/// lives one level down, where a set's node comes out as the header plus the key and not
/// a byte more.
///
/// The hasher and the equality are fixed at construction and travel with the keys through
/// copy and swap, as in ds/hmap. They must agree: keys equal under 'eq' must hash alike.
///
/// What this adds over the map it wraps is a NARROWER interface, not an invariant over
/// the keys: there is no value to get, to set or to hand out. An entry is either in or out.
///
/// A key never moves: growing relinks the nodes rather than moving them, so a borrowed
/// node survives everything but the removal of that very key. The iteration order is
/// unspecified and may change on any insert that grows.
///
/// @par Example
/// @snippet ds/example_hset.c build
/// @snippet ds/example_hset.c membership
/// @snippet ds/example_hset.c walk
/// @{

/// Owning hash set of type-erased keys.
/// An opaque handle: it comes from a constructor and goes back to nad_hset_drop
typedef struct nad_HSet nad_HSet;

/// A position in the set.
/// A set entry IS a map entry — the same node, seen through an API that offers no value
/// side — so the two spell the position with one type
typedef nad_HMapNode nad_HSetNode;

/// @name lifetime
/// @{

/// an empty set that owns no buckets yet
/// @param key_size the size of one key, asserted greater than 0
/// @param hasher the hash the keys are placed by, fixed for the life of the set
/// @param eq the equality the keys are matched by; must agree with 'hasher'
/// @param al the allocator, kept for everything after
/// @param[out] out the new set, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header cannot be allocated
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Status nad_hset_new(size_t key_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al, nad_HSet **out);

/// an empty set with room for 'cap' keys before the first growth
/// @param cap how many keys to make room for
/// @param key_size the size of one key, asserted greater than 0
/// @param hasher the hash the keys are placed by
/// @param eq the equality the keys are matched by
/// @param al the allocator
/// @param[out] out the new set, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the buckets cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_hset_new_cap(size_t cap, size_t key_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al, nad_HSet **out);

/// releases every node, the buckets and the set through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object
/// @bigo{n}
NAD_API
void nad_hset_drop(nad_HSet *self);

/// @}

/// @name copy
/// @{

/// a new set with the same keys, hasher and equality, on the same allocator
/// @param self the set to copy
/// @param[out] out the new set, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the buckets or any node cannot be allocated
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_hset_copy(const nad_HSet *self, nad_HSet **out);

/// overwrites the keys of 'other' with those of 'self'
/// @param self the set to copy from
/// @param[in,out] other must have the same key_size; receives the hasher and the equality
///                      along with the keys, overwriting its own, and keeps its own
///                      allocator. 'self' == 'other' is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when a node cannot be allocated, leaving 'other' as
///         it was
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_hset_copy_assign(const nad_HSet *self, nad_HSet *other);

/// @}

/// @name info
/// @{

/// how many keys the set holds
/// @param self the set
/// @return the count of entries, not of buckets
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_hset_len(const nad_HSet *self);

/// how many buckets the keys are spread over
/// @param self the set
/// @return the bucket count, a power of two, or 0 while the set owns none
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_hset_bucket_count(const nad_HSet *self);

/// the size of one key, as named at construction
/// @param self the set
/// @return key_size, which never moves
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_hset_key_size(const nad_HSet *self);

/// the allocator the set was built with
/// @param self the set
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_hset_al(const nad_HSet *self);

/// the hash the keys are placed by
/// @param self the set
/// @return the hasher, as given at construction
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Hasher nad_hset_hasher(const nad_HSet *self);

/// the equality the keys are matched by
/// @param self the set
/// @return the equality, as given at construction. Named key_eq because nad_hset_eq
///         compares two whole sets
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Eq nad_hset_key_eq(const nad_HSet *self);

/// @}

/// @name compare
/// @{

/// whether the two hold the same keys, matched by the hasher and the equality of 'b'
/// @param a one set
/// @param b must have the same key_size; a differing length is just false
/// @return whether every key of 'a' is in 'b' and the lengths agree, which is containment
///         both ways
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_hset_eq(const nad_HSet *a, const nad_HSet *b);

/// @}

/// @name lookup
/// @{

/// whether 'key' is in the set
/// @param self the set
/// @param key the key to look for
/// @return whether an entry matched it
/// @bigo{1} expected
[[nodiscard]] NAD_API
bool nad_hset_contains(const nad_HSet *self, const void *key);

/// the entry for 'key' as a position
/// @param self the set
/// @param key the key to look for
/// @return the node, or null when the set holds no such key
/// @bigo{1} expected
[[nodiscard]] NAD_API
const nad_HSetNode *nad_hset_find(const nad_HSet *self, const void *key);

/// the entry for 'key' as a position to remove through
/// @copydetails nad_hset_find
[[nodiscard]] NAD_API
nad_HSetNode *nad_hset_find_mut(nad_HSet *self, const void *key);

/// @}

/// @name nodes
/// @{

/// the first entry of a walk
/// @param self the set
/// @return a node, or null while the set is empty. The order is unspecified
/// @bigo{n} in the bucket count
[[nodiscard]] NAD_API
const nad_HSetNode *nad_hset_first_node(const nad_HSet *self);

/// the first entry of a walk, to remove through
/// @copydetails nad_hset_first_node
[[nodiscard]] NAD_API
nad_HSetNode *nad_hset_first_node_mut(nad_HSet *self);

/// the entry after 'node'
/// @param self the set, needed because a chain ends long before the buckets do
/// @param node the position to step from
/// @return the next node, or null at the end
/// @bigo{1} amortized over a whole walk
[[nodiscard]] NAD_API
const nad_HSetNode *nad_hset_node_next(const nad_HSet *self, const nad_HSetNode *node);

/// the entry after 'node', to remove through
/// @copydetails nad_hset_node_next
[[nodiscard]] NAD_API
nad_HSetNode *nad_hset_node_next_mut(nad_HSet *self, nad_HSetNode *node);

/// the key at 'node'
/// @param node the position
/// @return a read-only pointer to the key, and there is nothing beside it. Read only
///         because a written key would belong to a bucket the set no longer chose for it
/// @bigo{1}
[[nodiscard]] NAD_API
const void *nad_hset_node_key(const nad_HSetNode *node);

/// @}

/// @name mods
/// @{

/// puts 'key' in the set
/// @param self the set
/// @param key the key to copy in; an equal one already there is left as it is
/// @param[out] out_is_new whether the key was absent; may be null when the caller does
///                        not care, and written only on NAD_STATUS_OK
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the node or the wider bucket array cannot be
///         allocated
/// @bigo{1} expected, plus the amortized cost of growing
[[nodiscard]] NAD_API
nad_Status nad_hset_insert(nad_HSet *self, const void *key, bool *out_is_new);

/// drops 'key' if it is there
/// @param self the set
/// @param key the key to drop
/// @return whether it was there. Nothing is allocated, so nothing can fail
/// @bigo{1} expected
NAD_API
bool nad_hset_remove(nad_HSet *self, const void *key);

/// drops the entry 'node' names
/// @param self the set
/// @param node a position, asserted to be one of this set's own; every pointer into it
///             dies here, and no other entry is touched
/// @bigo{1}
NAD_API
void nad_hset_remove_node(nad_HSet *self, nad_HSetNode *node);

/// drops every key, keeping the buckets
/// @param self the set
/// @bigo{n}
NAD_API
void nad_hset_clear(nad_HSet *self);

/// makes room for 'cap' keys before the next growth
/// @param self the set
/// @param cap a capacity at or below the one it has is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the bucket array cannot be allocated, leaving
///         the set as it was
/// @bigo{n} — the nodes are relinked, not rebuilt, so a borrowed one survives this
[[nodiscard]] NAD_API
nad_Status nad_hset_reserve(nad_HSet *self, size_t cap);

/// gives back the buckets the keys no longer need
/// @param self an empty set is left owning nothing
/// @retval NAD_STATUS_OK on success, and when there was nothing to give back
/// @retval NAD_STATUS_ERR_NO_MEM when the smaller bucket array cannot be allocated,
///         leaving the set as it was
/// @bigo{n} — the nodes are relinked, not rebuilt, so a borrowed one survives this
[[nodiscard]] NAD_API
nad_Status nad_hset_shrink_to_fit(nad_HSet *self);

/// exchanges the contents of the two, hashers and equalities included
/// @param self one set
/// @param other must have the same key_size and the same allocator: the nodes change set
///              without moving, so every borrowed node stays valid — the same trade
///              ds/hmap and ds/list make
/// @bigo{1}
NAD_API
void nad_hset_swap(nad_HSet *self, nad_HSet *other);

/// @}

/// @name print
/// @{

/// writes the keys to a stream as {a, b, c}, followed by a newline, in the walk's order
/// @param self the set
/// @param stream where to write
/// @param fprint the printer, called once per key
/// @bigo{n}
NAD_API
void nad_hset_fprint(const nad_HSet *self, FILE *stream, nad_FPrint fprint);

/// nad_hset_fprint to stdout
/// @param self the set
/// @param fprint the printer, called once per key
/// @bigo{n}
NAD_API
void nad_hset_print(const nad_HSet *self, nad_FPrint fprint);

/// @}

/// @name macros
/// @{

/// nad_hset_new with sizeof(K) for the key size
/// @param K the key type
/// @param hasher the hash the keys are placed by
/// @param eq the equality the keys are matched by
/// @param al the allocator
/// @param[out] out where the new set is written
/// @bigo{1}
#define NAD_HSET_NEW(K, hasher, eq, al, out) \
    nad_hset_new(sizeof(K), (hasher), (eq), (al), (out))

/// nad_hset_new_cap with sizeof(K)
/// @param K the key type
/// @param cap how many keys to make room for
/// @param hasher the hash the keys are placed by
/// @param eq the equality the keys are matched by
/// @param al the allocator
/// @param[out] out where the new set is written
/// @bigo{n}
#define NAD_HSET_NEW_CAP(K, cap, hasher, eq, al, out) \
    nad_hset_new_cap((cap), sizeof(K), (hasher), (eq), (al), (out))

/// nad_hset_insert from a value rather than an address
/// @param K the key type; a scalar, since 'key' becomes a compound literal
/// @param self the set
/// @param key the value to copy in
/// @param[out] out_is_new whether the key was absent; may be null
/// @bigo{1} expected
#define NAD_HSET_INSERT(K, self, key, out_is_new) \
    nad_hset_insert((self), &(K){ (key) }, (out_is_new))

/// nad_hset_contains from a value rather than an address
/// @param K the key type; a scalar, since 'key' becomes a compound literal
/// @param self the set
/// @param key the value to look for
/// @bigo{1} expected
#define NAD_HSET_CONTAINS(K, self, key) \
    nad_hset_contains((self), &(K){ (key) })

/// nad_hset_find from a value rather than an address
/// @copydetails NAD_HSET_CONTAINS
#define NAD_HSET_FIND(K, self, key) \
    nad_hset_find((self), &(K){ (key) })

/// nad_hset_remove from a value rather than an address
/// @copydetails NAD_HSET_CONTAINS
#define NAD_HSET_REMOVE(K, self, key) \
    nad_hset_remove((self), &(K){ (key) })

/// nad_hset_node_key as a const K *
/// @param K the key type
/// @param node the position
/// @bigo{1}
#define NAD_HSET_NODE_KEY_AS(K, node) \
    ((const K *) nad_hset_node_key((node)))

/// @}

/// @}
