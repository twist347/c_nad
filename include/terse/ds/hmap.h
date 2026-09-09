#pragma once

#include "terse/alloc/alloc.h"
#include "terse/core/cmp.h"
#include "terse/core/export.h"
#include "terse/core/hash.h"
#include "terse/core/print.h"
#include "terse/core/status.h"

#include <stddef.h>

/// @file

/// @defgroup ds_hmap ds/hmap
/// @ingroup ds
/// @brief trs_HMap — an owning hash map from keys to values
///
/// Built on separate chaining: an array of buckets, each holding a chain of nodes that
/// hashed to it.
///
/// The hasher and the equality are fixed at construction and travel with the entries
/// through copy and swap — a bucket means nothing without the hash that chose it. They
/// must agree: keys equal under 'eq' must hash alike.
///
/// An entry never moves. Growing reallocates the bucket array and RELINKS the nodes into
/// it, so a borrowed node survives every operation but the removal of that very entry.
/// That is why a position here is a node rather than a slot index.
///
/// The iteration order is unspecified and may change on any insert that grows.
///
/// @par Example
/// @snippet ds/example_hmap.c build
/// @snippet ds/example_hmap.c lookup
/// @snippet ds/example_hmap.c count
/// @snippet ds/example_hmap.c walk
/// @{

/// Owning hash map from type-erased keys to type-erased values.
/// An opaque handle: it comes from a constructor and goes back to trs_hmap_drop
typedef struct trs_HMap trs_HMap;

/// A position in the map.
/// Borrowed from it and invalidated only by removing that very entry. Its key is readable
/// but never writable — a written key would belong to a bucket the map no longer chose
/// for it, with no way for the map to notice
typedef struct trs_HMapNode trs_HMapNode;

/// @name lifetime
/// @{

/// an empty map that owns no buckets yet
/// @param key_size the size of one key, asserted greater than 0
/// @param val_size the size of one value, asserted greater than 0 — a map with nothing on
///                 the value side is ds/hset
/// @param hasher the hash the keys are placed by, fixed for the life of the map
/// @param eq the equality the keys are matched by; must agree with 'hasher'
/// @param al the allocator, kept for everything after
/// @param[out] out the new map, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the header cannot be allocated
/// @bigo{1}
[[nodiscard]] TRS_API
trs_Status trs_hmap_new(size_t key_size, size_t val_size, trs_Hasher hasher, trs_Eq eq, trs_Al *al, trs_HMap **out);

/// an empty map with room for 'cap' entries before the first growth
/// @param cap how many entries to make room for
/// @param key_size the size of one key, asserted greater than 0
/// @param val_size the size of one value, asserted greater than 0
/// @param hasher the hash the keys are placed by
/// @param eq the equality the keys are matched by
/// @param al the allocator
/// @param[out] out the new map, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the buckets cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_hmap_new_cap(
    size_t cap,
    size_t key_size, size_t val_size,
    trs_Hasher hasher, trs_Eq eq,
    trs_Al *al,
    trs_HMap **out
);

/// releases every node, the buckets and the map through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object
/// @bigo{n}
TRS_API
void trs_hmap_drop(trs_HMap *self);

/// @}

/// @name copy
/// @{

/// a new map with the same entries, hasher and equality, on the same allocator
/// @param self the map to copy
/// @param[out] out the new map, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the buckets or any node cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_hmap_copy(const trs_HMap *self, trs_HMap **out);

/// a new map with the same entries, hasher and equality, on 'al'
/// @param self the map to copy
/// @param al where the copy lives; trs_hmap_copy is this one with the allocator of 'self'
/// @param[out] out the new map, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the buckets or any node cannot be allocated
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_hmap_copy_with(const trs_HMap *self, trs_Al *al, trs_HMap **out);

/// overwrites the entries of 'other' with those of 'self'
/// @param self the map to copy from
/// @param[in,out] other must have the same key_size and val_size; receives the hasher and
///                      the equality along with the entries, overwriting its own, and
///                      keeps its own allocator. 'self' == 'other' is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when a node cannot be allocated, leaving 'other' as
///         it was
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_hmap_copy_assign(const trs_HMap *self, trs_HMap *other);

/// moves the entries of 'self' into 'other', leaving 'self' empty
/// @param[in,out] self the map to move from; emptied on success and still usable, on
///                     its own allocator
/// @param[in,out] other must have the same key_size and val_size; receives the hasher and the
///                      equality along with the entries, releases what it held and keeps
///                      its own allocator. 'self' == 'other' is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the two sit on different allocators and the buckets or a node cannot be allocated,
///         leaving both as they were
/// @bigo{1} on one allocator, n on two — a node belongs to the allocator that made it,
///          so across two the entries are rebuilt and the borrowed nodes do not survive
[[nodiscard]] TRS_API
trs_Status trs_hmap_move_assign(trs_HMap *self, trs_HMap *other);

/// @}

/// @name info
/// @{

/// how many entries the map holds
/// @param self the map
/// @return the count of entries, not of buckets
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_hmap_len(const trs_HMap *self);

/// how many buckets the entries are spread over
/// @param self the map
/// @return the bucket count, always a power of two, and 0 while the map has never held
///         anything
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_hmap_bucket_count(const trs_HMap *self);

/// the size of one key, as named at construction
/// @param self the map
/// @return key_size, which never moves
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_hmap_key_size(const trs_HMap *self);

/// the size of one value, as named at construction
/// @param self the map
/// @return val_size, which never moves
/// @bigo{1}
[[nodiscard]] TRS_API
size_t trs_hmap_val_size(const trs_HMap *self);

/// the allocator the map was built with
/// @param self the map
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] TRS_API
trs_Al *trs_hmap_al(const trs_HMap *self);

/// the hash the keys are placed by
/// @param self the map
/// @return the hasher, as given at construction
/// @bigo{1}
[[nodiscard]] TRS_API
trs_Hasher trs_hmap_hasher(const trs_HMap *self);

/// the equality the keys are matched by
/// @param self the map
/// @return the equality, as given at construction. Named key_eq because trs_hmap_eq
///         compares two whole maps, and because the value side has no equality of its own
/// @bigo{1}
[[nodiscard]] TRS_API
trs_Eq trs_hmap_key_eq(const trs_HMap *self);

/// @}

/// @name compare
/// @{

/// whether the two hold the same entries: the same keys, each with an equal value
/// @param a one map
/// @param b must have the same key_size and val_size; a differing length is just false.
///          Keys are matched by the hasher and the equality of 'b'
/// @return whether the lengths agree and every entry of 'a' is in 'b' with the same value
///         byte for byte — the one comparison the caller cannot hand over as a trs_Eq,
///         which takes two pointers and no size
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_hmap_eq(const trs_HMap *a, const trs_HMap *b);

/// the same, with 'val_eq' deciding what an equal value is
/// @param a one map
/// @param b must have the same key_size and val_size
/// @param val_eq the caller's to name: a map carries no equality for its value side, and
///               this is what a value with padding or a field that does not count needs
/// @return whether the lengths agree and every entry of 'a' is in 'b' with an equal value
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_hmap_eq_by(const trs_HMap *a, const trs_HMap *b, trs_Eq val_eq);

/// @}

/// @name lookup
/// @{

/// the value stored under 'key'
/// @param self the map
/// @param key the key to look for
/// @return a pointer to the value, or null when the map holds no such key. A miss is not
///         an error and has exactly one cause, so it needs no status of its own
/// @bigo{1} expected
[[nodiscard]] TRS_API
const void *trs_hmap_get(const trs_HMap *self, const void *key);

/// the value stored under 'key', to write through
/// @copydetails trs_hmap_get
[[nodiscard]] TRS_API
void *trs_hmap_get_mut(trs_HMap *self, const void *key);

/// whether the map holds an entry under 'key'
/// @param self the map
/// @param key the key to look for
/// @return whether an entry matched it
/// @bigo{1} expected
[[nodiscard]] TRS_API
bool trs_hmap_contains(const trs_HMap *self, const void *key);

/// the entry under 'key' as a position
/// @param self the map
/// @param key the key to look for
/// @return the node, or null when there is none. Hold it to reach the key and the value
///         together, or to remove the entry without hashing it again
/// @bigo{1} expected
[[nodiscard]] TRS_API
const trs_HMapNode *trs_hmap_find(const trs_HMap *self, const void *key);

/// the entry under 'key' as a position to write or remove through
/// @copydetails trs_hmap_find
[[nodiscard]] TRS_API
trs_HMapNode *trs_hmap_find_mut(trs_HMap *self, const void *key);

/// @}

/// @name nodes
/// @{

/// the first entry of a walk
/// @param self the map
/// @return a node, or null on an empty map. The order is the buckets' and is unspecified
/// @bigo{n} in the bucket count
[[nodiscard]] TRS_API
const trs_HMapNode *trs_hmap_first_node(const trs_HMap *self);

/// the first entry of a walk, to write or remove through
/// @copydetails trs_hmap_first_node
[[nodiscard]] TRS_API
trs_HMapNode *trs_hmap_first_node_mut(trs_HMap *self);

/// the entry after 'node'
/// @param self the map, needed because a chain ends long before the buckets do — unlike a
///             list node, which knows its own successor
/// @param node the position to step from
/// @return the next node, or null at the end
/// @bigo{1} amortized over a whole walk
[[nodiscard]] TRS_API
const trs_HMapNode *trs_hmap_node_next(const trs_HMap *self, const trs_HMapNode *node);

/// the entry after 'node', to write or remove through
/// @copydetails trs_hmap_node_next
[[nodiscard]] TRS_API
trs_HMapNode *trs_hmap_node_next_mut(trs_HMap *self, trs_HMapNode *node);

/// the key at 'node'
/// @param node the position; the key sits at the front of it, so this one needs nothing
///             else
/// @return a read-only pointer to the key. There is no node_key_mut to match node_val_mut:
///         the value is the map's to hand over, the key is not
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_hmap_node_key(const trs_HMapNode *node);

/// the value at 'node'
/// @param self the map, needed for a different reason than in node_next: the value sits
///             behind the key, and how far behind is a property of the map's key size
///             rather than of the node
/// @param node the position
/// @return a pointer to the value
/// @bigo{1}
[[nodiscard]] TRS_API
const void *trs_hmap_node_val(const trs_HMap *self, const trs_HMapNode *node);

/// the value at 'node', to write through
/// @copydetails trs_hmap_node_val
[[nodiscard]] TRS_API
void *trs_hmap_node_val_mut(const trs_HMap *self, trs_HMapNode *node);

/// @}

/// @name mods
/// @{

/// stores 'val' under 'key', overwriting the value already there when the key is present
/// @param self the map
/// @param key the key to copy in
/// @param val the value to copy in
/// @param[out] out_is_new whether the key was absent; may be null when the caller does
///                        not care, and written only on TRS_STATUS_OK. It reports an
///                        observation rather than the operation's result, which is the
///                        map itself
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the node or the wider bucket array cannot be
///         allocated
/// @bigo{1} expected, plus the amortized cost of growing
[[nodiscard]] TRS_API
trs_Status trs_hmap_insert(trs_HMap *self, const void *key, const void *val, bool *out_is_new);

/// drops the entry under 'key' if there is one
/// @param self the map
/// @param key the key to drop
/// @return whether there was an entry. Nothing is allocated, so nothing can fail
/// @bigo{1} expected
TRS_API
bool trs_hmap_remove(trs_HMap *self, const void *key);

/// drops the entry 'node' names
/// @param self the map
/// @param node a position, asserted to be one of this map's own; every pointer into it
///             dies here, and no other entry is touched. The bucket is found from the
///             hash the node carries, so the key is never hashed again
/// @bigo{1}
TRS_API
void trs_hmap_remove_node(trs_HMap *self, trs_HMapNode *node);

/// hands back the entry for 'key', putting 'val_if_absent' there first when the key was
/// missing. This is the counter idiom's operation
/// @param self the map
/// @param key the key to look for or copy in
/// @param val_if_absent the value to start from when the key is not there; ignored when
///                      it is
/// @param[out] out_node the entry either way, written only on TRS_STATUS_OK
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the node or the wider bucket array cannot be
///         allocated
/// @bigo{1} expected — one hash and one walk of the bucket either way, which a lookup
///          followed by an insert cannot be: that pair hashes twice and walks twice
///          whenever the key turns out to be absent
[[nodiscard]] TRS_API
trs_Status trs_hmap_get_or_insert(
    trs_HMap *self,
    const void *key,
    const void *val_if_absent,
    trs_HMapNode **out_node
);

/// drops every entry, keeping the buckets
/// @param self the map
/// @bigo{n}
TRS_API
void trs_hmap_clear(trs_HMap *self);

/// makes room for 'cap' entries without a growth in between
/// @param self the map
/// @param cap a capacity at or below the one it has is a no-op
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when the bucket array cannot be allocated, leaving
///         the map as it was
/// @bigo{n} — only the bucket array is reallocated, the entries are relinked where they
///            lie, so a borrowed node survives this
[[nodiscard]] TRS_API
trs_Status trs_hmap_reserve(trs_HMap *self, size_t cap);

/// gives back the buckets the entries no longer need, down to the smallest power of two
/// that still holds them and never below the count a fresh map takes
/// @param self an empty map is left owning nothing at all
/// @retval TRS_STATUS_OK on success, and when there was nothing to give back
/// @retval TRS_STATUS_ERR_NO_MEM when the smaller bucket array cannot be allocated,
///         leaving the map as it was
/// @bigo{n} — the entries are relinked, not rebuilt, so a borrowed node survives this as
///            it survives a growth
[[nodiscard]] TRS_API
trs_Status trs_hmap_shrink_to_fit(trs_HMap *self);

/// exchanges the contents of the two, hashers and equalities included
/// @param self one map
/// @param other must have the same key_size and val_size and the same allocator: the
///              nodes change map without moving, so every borrowed node stays valid — the
///              same trade ds/list makes. Two maps built under different hashers stay
///              valid maps afterwards, since each order travels with its entries
/// @note two allocators are a broken precondition, not a runtime state — a node belongs to
///       the allocator that made it. To exchange across two, build each side on the other's
///       allocator with trs_hmap_copy_with and hand the results over with
///       trs_hmap_move_assign
/// @bigo{1}
TRS_API
void trs_hmap_swap(trs_HMap *self, trs_HMap *other);

/// @}

/// @name print
/// @{

/// writes the entries to a stream as {a: 1, b: 2}, followed by a newline, in the walk's
/// order
/// @param self the map
/// @param stream where to write
/// @param key_fprint the printer for a key
/// @param val_fprint the printer for a value
/// @bigo{n}
TRS_API
void trs_hmap_fprint(const trs_HMap *self, FILE *stream, trs_FPrint key_fprint, trs_FPrint val_fprint);

/// trs_hmap_fprint to stdout
/// @param self the map
/// @param key_fprint the printer for a key
/// @param val_fprint the printer for a value
/// @bigo{n}
TRS_API
void trs_hmap_print(const trs_HMap *self, trs_FPrint key_fprint, trs_FPrint val_fprint);

/// @}

/// @name macros
/// @{

/// trs_hmap_new with sizeof(K) and sizeof(V)
/// @param K the key type
/// @param V the value type
/// @param hasher the hash the keys are placed by
/// @param eq the equality the keys are matched by
/// @param al the allocator
/// @param[out] out where the new map is written
/// @bigo{1}
#define TRS_HMAP_NEW(K, V, hasher, eq, al, out) \
    trs_hmap_new(sizeof(K), sizeof(V), (hasher), (eq), (al), (out))

/// trs_hmap_new_cap with sizeof(K) and sizeof(V)
/// @param K the key type
/// @param V the value type
/// @param cap how many entries to make room for
/// @param hasher the hash the keys are placed by
/// @param eq the equality the keys are matched by
/// @param al the allocator
/// @param[out] out where the new map is written
/// @bigo{n}
#define TRS_HMAP_NEW_CAP(K, V, cap, hasher, eq, al, out) \
    trs_hmap_new_cap((cap), sizeof(K), sizeof(V), (hasher), (eq), (al), (out))

/// trs_hmap_get as a const V *, from a key value rather than an address
/// @param K the key type; a scalar, since 'key' becomes a compound literal
/// @param V the value type
/// @param self the map
/// @param key the value to look for
/// @bigo{1} expected
#define TRS_HMAP_GET_AS(K, V, self, key) \
    ((const V *) trs_hmap_get((self), &(K){ (key) }))

/// trs_hmap_get_mut as a V *
/// @copydetails TRS_HMAP_GET_AS
#define TRS_HMAP_GET_MUT_AS(K, V, self, key) \
    ((V *) trs_hmap_get_mut((self), &(K){ (key) }))

/// trs_hmap_contains from a key value rather than an address
/// @param K the key type; a scalar, since 'key' becomes a compound literal
/// @param self the map
/// @param key the value to look for
/// @bigo{1} expected
#define TRS_HMAP_CONTAINS(K, self, key) \
    trs_hmap_contains((self), &(K){ (key) })

/// trs_hmap_find from a key value rather than an address
/// @copydetails TRS_HMAP_CONTAINS
#define TRS_HMAP_FIND(K, self, key) \
    trs_hmap_find((self), &(K){ (key) })

/// trs_hmap_insert from values rather than addresses
/// @param K the key type; a scalar, since 'key' becomes a compound literal
/// @param V the value type; a scalar, for the same reason
/// @param self the map
/// @param key the key value to copy in
/// @param val the value to copy in
/// @param[out] out_is_new whether the key was absent; may be null
/// @bigo{1} expected
#define TRS_HMAP_INSERT(K, V, self, key, val, out_is_new) \
    trs_hmap_insert((self), &(K){ (key) }, &(V){ (val) }, (out_is_new))

/// trs_hmap_get_or_insert from values rather than addresses
/// @param K the key type; a scalar, since 'key' becomes a compound literal
/// @param V the value type; a scalar, for the same reason
/// @param self the map
/// @param key the key value to look for or copy in
/// @param val the value to start from when the key is absent
/// @param[out] out_node the entry either way
/// @bigo{1} expected
#define TRS_HMAP_GET_OR_INSERT(K, V, self, key, val, out_node) \
    trs_hmap_get_or_insert((self), &(K){ (key) }, &(V){ (val) }, (out_node))

/// trs_hmap_remove from a key value rather than an address
/// @copydetails TRS_HMAP_CONTAINS
#define TRS_HMAP_REMOVE(K, self, key) \
    trs_hmap_remove((self), &(K){ (key) })

/// walks every entry of the map, binding 'node' to each in turn. The order is
/// unspecified: it follows the buckets, not the insertions
/// @param node the name the loop variable takes; it is a const trs_HMapNode *
/// @param self the map
/// @note the step to the next entry happens after the body, through the node the body
///       saw — so removing that entry inside the loop cuts the walk
#define TRS_HMAP_FOR_EACH(node, self)                                \
    for (const trs_HMapNode *node = trs_hmap_first_node(self);       \
         node;                                                       \
         node = trs_hmap_node_next((self), node))

/// the same walk over entries whose values may be written through
/// @copydetails TRS_HMAP_FOR_EACH
#define TRS_HMAP_FOR_EACH_MUT(node, self)                            \
    for (trs_HMapNode *node = trs_hmap_first_node_mut(self);         \
         node;                                                       \
         node = trs_hmap_node_next_mut((self), node))

/// trs_hmap_node_key as a const K *
/// @param K the key type
/// @param node the position
/// @bigo{1}
#define TRS_HMAP_NODE_KEY_AS(K, node) \
    ((const K *) trs_hmap_node_key((node)))

/// trs_hmap_node_val as a const V *
/// @param V the value type
/// @param self the map
/// @param node the position
/// @bigo{1}
#define TRS_HMAP_NODE_VAL_AS(V, self, node) \
    ((const V *) trs_hmap_node_val((self), (node)))

/// trs_hmap_node_val_mut as a V *
/// @copydetails TRS_HMAP_NODE_VAL_AS
#define TRS_HMAP_NODE_VAL_MUT_AS(V, self, node) \
    ((V *) trs_hmap_node_val_mut((self), (node)))

/// @}

/// @}
