#include "terse/ds/hmap.h"

#include "terse/core/util.h"

#include "internal/hmap_impl.h"
#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

/* ========== internals ========== */

#define ASSERT_HMAP(m)                                                          \
    (assert(m),                                                                 \
     assert((m)->key_size > 0),                                                 \
     assert((m)->hasher),                                                       \
     assert((m)->eq),                                                           \
     assert((m)->al),                                                           \
     assert(((m)->bucket_count == 0) == ((m)->buckets == nullptr)),             \
     assert((m)->bucket_count == 0                                              \
            || ((m)->bucket_count & ((m)->bucket_count - 1)) == 0),             \
     assert((m)->len == 0 || (m)->bucket_count > 0))

static constexpr size_t HMAP_BUCKETS_BASE = 8;
static constexpr size_t HMAP_GROWTH_FACTOR = 2;

// The key and the value share one flexible array so an entry is one allocation rather
// than two: the key sits at 0 and the value at 'val_offset', which is 'key_size' rounded
// up to the widest alignment the platform has. The hash is kept because it pays twice —
// growing relinks without asking the hasher again, and a lookup rejects on a number
// before it ever calls 'eq'.
struct trs_HMapNode {
    trs_HMapNode *next;
    trs_Hash hash;
    alignas(max_align_t) unsigned char kv[];
};

struct trs_HMap {
    trs_HMapNode **buckets;
    size_t bucket_count;
    size_t len;
    size_t key_size;
    size_t val_size;
    size_t val_offset;
    trs_Hasher hasher;
    trs_Eq eq;
    trs_Al *al;
};

[[nodiscard]]
static size_t node_bytes(const trs_HMap *self);

[[nodiscard]]
static const void *node_key(const trs_HMapNode *node);

[[nodiscard]]
static void *node_val_mut(const trs_HMap *self, trs_HMapNode *node);

[[nodiscard]]
static const void *node_val(const trs_HMap *self, const trs_HMapNode *node);

[[nodiscard]]
static trs_Status node_new(const trs_HMap *self, const void *key, const void *val, trs_Hash hash, trs_HMapNode **out);

static void node_drop(const trs_HMap *self, trs_HMapNode *node);

/// which bucket a hash belongs to. The count is a power of two, so this is a mask and not
/// a division — affordable only because the mixer in core/hash gives every bit avalanche
[[nodiscard]]
static size_t bucket_of(const trs_HMap *self, trs_Hash hash);

[[nodiscard]]
static trs_HMapNode *find_node(const trs_HMap *self, const void *key, trs_Hash hash);

/// builds the entry for a key the caller has already found to be absent and links it into
/// its bucket. Shared by insert and get_or_insert, which differ only in what they do when
/// the key IS there
[[nodiscard]]
static trs_Status add_node(trs_HMap *self, const void *key, const void *val, trs_Hash hash, trs_HMapNode **out);

/// the smallest power of two that is at least 'want', or 0 on overflow
[[nodiscard]]
static size_t round_up_pow2(size_t want);

/// moves every node into a fresh bucket array of 'new_count'. Only the array is
/// allocated: the nodes are relinked where they lie, which is what keeps a borrowed node
/// valid across a growth
[[nodiscard]]
static trs_Status rehash(trs_HMap *self, size_t new_count);

/// room for one more entry, growing the buckets when the load would pass one per bucket
[[nodiscard]]
static trs_Status reserve_one(trs_HMap *self);

/// the first node from bucket 'idx' onward, or null when the rest are empty
[[nodiscard]]
static trs_HMapNode *first_from(const trs_HMap *self, size_t idx);

static void clear_nodes(trs_HMap *self);

/// the walk both compare doors take, with 'val_eq' null standing for the bytes
[[nodiscard]]
static bool eq_impl(const trs_HMap *a, const trs_HMap *b, trs_Eq val_eq);

/* ========== lifetime ========== */

trs_Status trs_hmap_new(size_t key_size, size_t val_size, trs_Hasher hasher, trs_Eq eq, trs_Al *al, trs_HMap **out) {
    return trs_hmap_new_cap(0, key_size, val_size, hasher, eq, al, out);
}

trs_Status trs_hmap_new_cap(
    size_t cap,
    size_t key_size, size_t val_size,
    trs_Hasher hasher, trs_Eq eq,
    trs_Al *al,
    trs_HMap **out
) {
    assert(val_size > 0); // the zero belongs to internal/hmap_impl.h and to ds/hset alone

    return trs_hmap_new_raw_(cap, key_size, val_size, hasher, eq, al, out);
}

trs_Status trs_hmap_new_raw_(
    size_t cap, size_t key_size, size_t val_size,
    trs_Hasher hasher, trs_Eq eq,
    trs_Al *al,
    trs_HMap **out
) {
    assert(key_size > 0);
    assert(hasher);
    assert(eq);
    assert(al);
    assert(out);

    // with no value to follow the key there is nothing to align it to, so a set's node is
    // the header plus the key and not a byte more
    const size_t val_offset = val_size == 0
                                  ? key_size
                                  : trs_align_up(key_size, alignof(max_align_t));

    size_t kv_bytes;
    if (ckd_add(&kv_bytes, val_offset, val_size)) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    trs_HMap *obj = trs_alloc(al, sizeof(trs_HMap));
    if (!obj) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    obj->buckets = nullptr;
    obj->bucket_count = 0;
    obj->len = 0;
    obj->key_size = key_size;
    obj->val_size = val_size;
    obj->val_offset = val_offset;
    obj->hasher = hasher;
    obj->eq = eq;
    obj->al = al;

    if (cap > 0) {
        const trs_Status st = trs_hmap_reserve(obj, cap);
        if (TRS_STATUS_IS_ERR(st)) {
            trs_dealloc(al, obj, sizeof(trs_HMap));
            return st;
        }
    }

    ASSERT_HMAP(obj);

    *out = obj;

    return TRS_STATUS_OK;
}

void trs_hmap_drop(trs_HMap *self) {
    if (!self) {
        return;
    }

    ASSERT_HMAP(self);

    trs_Al *al_copy = self->al;
    clear_nodes(self);
    trs_dealloc(al_copy, self->buckets, self->bucket_count * sizeof(trs_HMapNode *));
    trs_dealloc(al_copy, self, sizeof(trs_HMap));
}

/* ========== copy ========== */

trs_Status trs_hmap_copy(const trs_HMap *self, trs_HMap **out) {
    ASSERT_HMAP(self);

    return trs_hmap_copy_with(self, self->al, out);
}

trs_Status trs_hmap_copy_with(const trs_HMap *self, trs_Al *al, trs_HMap **out) {
    ASSERT_HMAP(self);
    assert(al);
    assert(out);

    trs_HMap *obj;
    trs_Status st = trs_hmap_new_raw_(self->len, self->key_size, self->val_size, self->hasher, self->eq, al, &obj);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    for (const trs_HMapNode *node = first_from(self, 0); node; node = trs_hmap_node_next(self, node)) {
        const void *val = self->val_size > 0 ? node_val(self, node) : nullptr;
        st = trs_hmap_insert(obj, node_key(node), val, nullptr);
        if (TRS_STATUS_IS_ERR(st)) {
            trs_hmap_drop(obj);
            return st;
        }
    }

    *out = obj;

    return TRS_STATUS_OK;
}

trs_Status trs_hmap_copy_assign(const trs_HMap *self, trs_HMap *other) {
    ASSERT_HMAP(self);
    ASSERT_HMAP(other);
    assert(self->key_size == other->key_size);
    assert(self->val_size == other->val_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    // the whole clone is built before anything of 'other' is touched, so a refusal
    // halfway through leaves the target exactly as it was
    trs_HMap *clone;
    const trs_Status st = trs_hmap_copy_with(self, other->al, &clone);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    TRS_SWAP(*other, *clone);
    trs_hmap_drop(clone);

    ASSERT_HMAP(other);

    return TRS_STATUS_OK;
}

trs_Status trs_hmap_move_assign(trs_HMap *self, trs_HMap *other) {
    ASSERT_HMAP(self);
    ASSERT_HMAP(other);
    assert(self->key_size == other->key_size);
    assert(self->val_size == other->val_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    // one allocator: the buckets are handed over, nodes and all. What 'other' held ends up in 'self' and is released
    // there, through the very allocator that made it
    if (self->al == other->al) {
        TRS_SWAP(*self, *other);
        trs_hmap_clear(self);

        ASSERT_HMAP(self);
        ASSERT_HMAP(other);

        return TRS_STATUS_OK;
    }

    // two allocators: the whole copy is built on the target's before anything of it is
    // touched, so a refusal leaves both as they were
    trs_HMap *obj;
    const trs_Status st = trs_hmap_copy_with(self, other->al, &obj);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    TRS_SWAP(*other, *obj);
    trs_hmap_drop(obj);
    trs_hmap_clear(self);

    ASSERT_HMAP(self);
    ASSERT_HMAP(other);

    return TRS_STATUS_OK;
}

/* ========== info ========== */

size_t trs_hmap_len(const trs_HMap *self) {
    ASSERT_HMAP(self);

    return self->len;
}

size_t trs_hmap_bucket_count(const trs_HMap *self) {
    ASSERT_HMAP(self);

    return self->bucket_count;
}

size_t trs_hmap_key_size(const trs_HMap *self) {
    ASSERT_HMAP(self);

    return self->key_size;
}

size_t trs_hmap_val_size(const trs_HMap *self) {
    ASSERT_HMAP(self);

    return self->val_size;
}

trs_Al *trs_hmap_al(const trs_HMap *self) {
    ASSERT_HMAP(self);

    return self->al;
}

trs_Hasher trs_hmap_hasher(const trs_HMap *self) {
    ASSERT_HMAP(self);

    return self->hasher;
}

trs_Eq trs_hmap_key_eq(const trs_HMap *self) {
    ASSERT_HMAP(self);

    return self->eq;
}

/* ========== compare ========== */

bool trs_hmap_eq(const trs_HMap *a, const trs_HMap *b) {
    ASSERT_HMAP(a);
    ASSERT_HMAP(b);
    assert(a->key_size == b->key_size);
    assert(a->val_size == b->val_size);

    return eq_impl(a, b, nullptr);
}

bool trs_hmap_eq_by(const trs_HMap *a, const trs_HMap *b, trs_Eq val_eq) {
    ASSERT_HMAP(a);
    ASSERT_HMAP(b);
    assert(a->key_size == b->key_size);
    assert(a->val_size == b->val_size);
    assert(val_eq);

    return eq_impl(a, b, val_eq);
}

/* ========== lookup ========== */

const void *trs_hmap_get(const trs_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    const trs_HMapNode *node = find_node(self, key, self->hasher(key));

    return node ? node_val(self, node) : nullptr;
}

void *trs_hmap_get_mut(trs_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    trs_HMapNode *node = find_node(self, key, self->hasher(key));

    return node ? node_val_mut(self, node) : nullptr;
}

bool trs_hmap_contains(const trs_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    return find_node(self, key, self->hasher(key)) != nullptr;
}

const trs_HMapNode *trs_hmap_find(const trs_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    return find_node(self, key, self->hasher(key));
}

trs_HMapNode *trs_hmap_find_mut(trs_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    return find_node(self, key, self->hasher(key));
}

/* ========== nodes ========== */

const trs_HMapNode *trs_hmap_first_node(const trs_HMap *self) {
    ASSERT_HMAP(self);

    return first_from(self, 0);
}

trs_HMapNode *trs_hmap_first_node_mut(trs_HMap *self) {
    ASSERT_HMAP(self);

    return first_from(self, 0);
}

const trs_HMapNode *trs_hmap_node_next(const trs_HMap *self, const trs_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);

    if (node->next) {
        return node->next;
    }

    return first_from(self, bucket_of(self, node->hash) + 1);
}

trs_HMapNode *trs_hmap_node_next_mut(trs_HMap *self, trs_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);

    if (node->next) {
        return node->next;
    }

    return first_from(self, bucket_of(self, node->hash) + 1);
}

const void *trs_hmap_node_key(const trs_HMapNode *node) {
    assert(node);

    return node_key(node);
}

const void *trs_hmap_node_val(const trs_HMap *self, const trs_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);

    return node_val(self, node);
}

void *trs_hmap_node_val_mut(const trs_HMap *self, trs_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);

    return node_val_mut(self, node);
}

/* ========== mods ========== */

trs_Status trs_hmap_insert(trs_HMap *self, const void *key, const void *val, bool *out_is_new) {
    ASSERT_HMAP(self);
    assert(key);
    assert(val || self->val_size == 0); // a value pointer is wanted exactly when there is a value

    const trs_Hash hash = self->hasher(key);

    trs_HMapNode *found = find_node(self, key, hash);
    if (found) {
        if (self->val_size > 0) {
            memcpy(node_val_mut(self, found), val, self->val_size);
        }

        if (out_is_new) {
            *out_is_new = false;
        }

        return TRS_STATUS_OK;
    }

    trs_HMapNode *node;
    const trs_Status st = add_node(self, key, val, hash, &node);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    if (out_is_new) {
        *out_is_new = true;
    }

    return TRS_STATUS_OK;
}

trs_Status trs_hmap_get_or_insert(
    trs_HMap *self,
    const void *key,
    const void *val_if_absent,
    trs_HMapNode **out_node
) {
    ASSERT_HMAP(self);
    assert(key);
    assert(val_if_absent || self->val_size == 0);
    assert(out_node);

    // one hash for both halves of the question, and one walk of the bucket it names
    const trs_Hash hash = self->hasher(key);

    trs_HMapNode *found = find_node(self, key, hash);
    if (found) {
        *out_node = found;
        return TRS_STATUS_OK;
    }

    return add_node(self, key, val_if_absent, hash, out_node);
}

bool trs_hmap_remove(trs_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    if (self->bucket_count == 0) {
        return false;
    }

    const trs_Hash hash = self->hasher(key);

    // walking the links themselves rather than the nodes: the chain is singly linked, and
    // this is what stands in for the previous node
    trs_HMapNode **link = &self->buckets[bucket_of(self, hash)];
    while (*link) {
        if ((*link)->hash == hash && self->eq(node_key(*link), key)) {
            trs_HMapNode *dead = *link;
            *link = dead->next;
            node_drop(self, dead);
            --self->len;

            ASSERT_HMAP(self);

            return true;
        }
        link = &(*link)->next;
    }

    return false;
}

void trs_hmap_remove_node(trs_HMap *self, trs_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);
    assert(self->len > 0);

    trs_HMapNode **link = &self->buckets[bucket_of(self, node->hash)];
    while (*link && *link != node) {
        link = &(*link)->next;
    }

    assert(*link == node); // the node must belong to this map

    *link = node->next;
    node_drop(self, node);
    --self->len;

    ASSERT_HMAP(self);
}

void trs_hmap_clear(trs_HMap *self) {
    ASSERT_HMAP(self);

    clear_nodes(self);

    for (size_t i = 0; i < self->bucket_count; ++i) {
        self->buckets[i] = nullptr;
    }

    self->len = 0;

    ASSERT_HMAP(self);
}

trs_Status trs_hmap_reserve(trs_HMap *self, size_t cap) {
    ASSERT_HMAP(self);

    if (cap <= self->bucket_count) {
        return TRS_STATUS_OK;
    }

    // round_up_pow2 never returns less than the base, so a small 'cap' still gets a
    // sensible bucket array rather than one or two buckets
    const size_t want = round_up_pow2(cap);
    if (want == 0) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    return rehash(self, want);
}

trs_Status trs_hmap_shrink_to_fit(trs_HMap *self) {
    ASSERT_HMAP(self);

    if (self->len == 0) {
        // nothing left to hold: the map goes back to owning no buckets at all
        trs_dealloc(self->al, self->buckets, self->bucket_count * sizeof(trs_HMapNode *));
        self->buckets = nullptr;
        self->bucket_count = 0;

        ASSERT_HMAP(self);

        return TRS_STATUS_OK;
    }

    const size_t want = round_up_pow2(self->len);
    if (want == 0 || want >= self->bucket_count) {
        return TRS_STATUS_OK;
    }

    return rehash(self, want);
}

void trs_hmap_swap(trs_HMap *self, trs_HMap *other) {
    ASSERT_HMAP(self);
    ASSERT_HMAP(other);
    assert(self->key_size == other->key_size);
    assert(self->val_size == other->val_size);
    assert(self->al == other->al);

    if (self == other) {
        return;
    }

    TRS_SWAP(*self, *other);

    ASSERT_HMAP(self);
    ASSERT_HMAP(other);
}

/* ========== print ========== */

void trs_hmap_fprint(const trs_HMap *self, FILE *stream, trs_FPrint key_fprint, trs_FPrint val_fprint) {
    ASSERT_HMAP(self);
    assert(stream);
    assert(key_fprint);
    assert(val_fprint);

    fputc('{', stream);
    bool first = true;
    for (const trs_HMapNode *node = first_from(self, 0); node; node = trs_hmap_node_next(self, node)) {
        if (!first) {
            fputs(", ", stream);
        }
        first = false;
        key_fprint(stream, node_key(node));
        fputs(": ", stream);
        val_fprint(stream, node_val(self, node));
    }
    fputs("}\n", stream);
}

void trs_hmap_print(const trs_HMap *self, trs_FPrint key_fprint, trs_FPrint val_fprint) {
    trs_hmap_fprint(self, stdout, key_fprint, val_fprint);
}

/* ========== internals ========== */

static size_t node_bytes(const trs_HMap *self) {
    return sizeof(trs_HMapNode) + self->val_offset + self->val_size;
}

static const void *node_key(const trs_HMapNode *node) {
    return node->kv;
}

static void *node_val_mut(const trs_HMap *self, trs_HMapNode *node) {
    return trs_byte_offset_mut(node->kv, 1, self->val_offset);
}

static const void *node_val(const trs_HMap *self, const trs_HMapNode *node) {
    return trs_byte_offset(node->kv, 1, self->val_offset);
}

static trs_Status node_new(const trs_HMap *self, const void *key, const void *val, trs_Hash hash, trs_HMapNode **out) {
    trs_HMapNode *node = trs_alloc(self->al, node_bytes(self));
    if (!node) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    assert(trs_ptr_is_aligned(node, alignof(max_align_t)));

    node->next = nullptr;
    node->hash = hash;
    memcpy(node->kv, key, self->key_size);
    if (self->val_size > 0) {
        memcpy(node_val_mut(self, node), val, self->val_size);
    }

    *out = node;

    return TRS_STATUS_OK;
}

static void node_drop(const trs_HMap *self, trs_HMapNode *node) {
    trs_dealloc(self->al, node, node_bytes(self));
}

static size_t bucket_of(const trs_HMap *self, trs_Hash hash) {
    assert(self->bucket_count > 0);

    return (size_t) hash & (self->bucket_count - 1);
}

static trs_HMapNode *find_node(const trs_HMap *self, const void *key, trs_Hash hash) {
    if (self->bucket_count == 0) {
        return nullptr;
    }

    for (trs_HMapNode *node = self->buckets[bucket_of(self, hash)]; node; node = node->next) {
        // the hash is compared first because it is a word: 'eq' is only asked about keys
        // that already agree on every mixed bit
        if (node->hash == hash && self->eq(node_key(node), key)) {
            return node;
        }
    }

    return nullptr;
}

static trs_Status add_node(trs_HMap *self, const void *key, const void *val, trs_Hash hash, trs_HMapNode **out) {
    // the room is taken first: growing relinks the buckets, so the one this node belongs
    // to is only known afterwards
    trs_Status st = reserve_one(self);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    trs_HMapNode *node;
    st = node_new(self, key, val, hash, &node);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    const size_t bucket = bucket_of(self, hash);
    node->next = self->buckets[bucket];
    self->buckets[bucket] = node;
    ++self->len;

    ASSERT_HMAP(self);

    *out = node;

    return TRS_STATUS_OK;
}

static size_t round_up_pow2(size_t want) {
    size_t n = HMAP_BUCKETS_BASE;
    while (n < want) {
        size_t grown;
        if (ckd_mul(&grown, n, HMAP_GROWTH_FACTOR)) {
            return 0;
        }
        n = grown;
    }

    return n;
}

static trs_Status rehash(trs_HMap *self, size_t new_count) {
    assert(new_count > 0);
    assert((new_count & (new_count - 1)) == 0);

    trs_HMapNode **buckets = trs_calloc(self->al, new_count, sizeof(trs_HMapNode *));
    if (!buckets) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    for (size_t i = 0; i < self->bucket_count; ++i) {
        trs_HMapNode *node = self->buckets[i];
        while (node) {
            trs_HMapNode *next = node->next;
            const size_t bucket = node->hash & (new_count - 1);
            node->next = buckets[bucket];
            buckets[bucket] = node;
            node = next;
        }
    }

    trs_dealloc(self->al, self->buckets, self->bucket_count * sizeof(trs_HMapNode *));
    self->buckets = buckets;
    self->bucket_count = new_count;

    ASSERT_HMAP(self);

    return TRS_STATUS_OK;
}

static trs_Status reserve_one(trs_HMap *self) {
    if (self->len < self->bucket_count) {
        return TRS_STATUS_OK;
    }

    if (self->bucket_count == 0) {
        return rehash(self, HMAP_BUCKETS_BASE);
    }

    size_t grown;
    if (ckd_mul(&grown, self->bucket_count, HMAP_GROWTH_FACTOR)) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    return rehash(self, grown);
}

static trs_HMapNode *first_from(const trs_HMap *self, size_t idx) {
    for (size_t i = idx; i < self->bucket_count; ++i) {
        if (self->buckets[i]) {
            return self->buckets[i];
        }
    }

    return nullptr;
}

static void clear_nodes(trs_HMap *self) {
    for (size_t i = 0; i < self->bucket_count; ++i) {
        trs_HMapNode *node = self->buckets[i];
        while (node) {
            trs_HMapNode *next = node->next;
            node_drop(self, node);
            node = next;
        }
    }
}

static bool eq_impl(const trs_HMap *a, const trs_HMap *b, trs_Eq val_eq) {
    if (a == b) {
        return true;
    }

    if (a->len != b->len) {
        return false;
    }

    // equal lengths plus every key of 'a' found in 'b' is containment both ways, so there
    // is no second pass. 'b' answers with its own hasher and equality: the keys are being
    // looked up in it
    for (size_t i = 0; i < a->bucket_count; ++i) {
        for (const trs_HMapNode *node = a->buckets[i]; node; node = node->next) {
            const void *key = node_key(node);
            const trs_HMapNode *found = find_node(b, key, b->hasher(key));
            if (!found) {
                return false;
            }

            const void *lhs = node_val(a, node);
            const void *rhs = node_val(b, found);
            const bool same = val_eq ? val_eq(lhs, rhs) : memcmp(lhs, rhs, a->val_size) == 0;
            if (!same) {
                return false;
            }
        }
    }

    return true;
}
