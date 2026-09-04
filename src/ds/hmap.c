#include "nad/ds/hmap.h"

#include "nad/core/util.h"
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
struct nad_HMapNode {
    nad_HMapNode *next;
    nad_Hash hash;
    alignas(max_align_t) unsigned char kv[];
};

struct nad_HMap {
    nad_HMapNode **buckets;
    size_t bucket_count;
    size_t len;
    size_t key_size;
    size_t val_size;
    size_t val_offset;
    nad_Hasher hasher;
    nad_Eq eq;
    nad_Al *al;
};

[[nodiscard]]
static size_t node_bytes(const nad_HMap *self);

[[nodiscard]]
static const void *node_key(const nad_HMapNode *node);

[[nodiscard]]
static void *node_val_mut(const nad_HMap *self, nad_HMapNode *node);

[[nodiscard]]
static const void *node_val(const nad_HMap *self, const nad_HMapNode *node);

[[nodiscard]]
static nad_Status node_new(const nad_HMap *self, const void *key, const void *val, nad_Hash hash, nad_HMapNode **out);

static void node_drop(const nad_HMap *self, nad_HMapNode *node);

/// which bucket a hash belongs to. The count is a power of two, so this is a mask and not
/// a division — affordable only because the mixer in core/hash gives every bit avalanche
[[nodiscard]]
static size_t bucket_of(const nad_HMap *self, nad_Hash hash);

[[nodiscard]]
static nad_HMapNode *find_node(const nad_HMap *self, const void *key, nad_Hash hash);

/// builds the entry for a key the caller has already found to be absent and links it into
/// its bucket. Shared by insert and get_or_insert, which differ only in what they do when
/// the key IS there
[[nodiscard]]
static nad_Status add_node(nad_HMap *self, const void *key, const void *val, nad_Hash hash, nad_HMapNode **out);

/// the smallest power of two that is at least 'want', or 0 on overflow
[[nodiscard]]
static size_t round_up_pow2(size_t want);

/// moves every node into a fresh bucket array of 'new_count'. Only the array is
/// allocated: the nodes are relinked where they lie, which is what keeps a borrowed node
/// valid across a growth
[[nodiscard]]
static nad_Status rehash(nad_HMap *self, size_t new_count);

/// room for one more entry, growing the buckets when the load would pass one per bucket
[[nodiscard]]
static nad_Status reserve_one(nad_HMap *self);

/// the first node from bucket 'start' onward, or null when the rest are empty
[[nodiscard]]
static nad_HMapNode *first_from(const nad_HMap *self, size_t start);

static void clear_nodes(nad_HMap *self);

/// the walk both compare doors take, with 'val_eq' null standing for the bytes
[[nodiscard]]
static bool eq_impl(const nad_HMap *a, const nad_HMap *b, nad_Eq val_eq);

/* ========== lifetime ========== */

nad_Status nad_hmap_new(size_t key_size, size_t val_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al, nad_HMap **out) {
    return nad_hmap_new_cap(0, key_size, val_size, hasher, eq, al, out);
}

nad_Status nad_hmap_new_cap(size_t cap, size_t key_size, size_t val_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al,
                            nad_HMap **out) {
    assert(val_size > 0); // the zero belongs to internal/hmap_impl.h and to ds/hset alone

    return nad_hmap_new_raw_(cap, key_size, val_size, hasher, eq, al, out);
}

nad_Status nad_hmap_new_raw_(size_t cap, size_t key_size, size_t val_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al,
                             nad_HMap **out) {
    assert(key_size > 0);
    assert(hasher);
    assert(eq);
    assert(al);
    assert(out);

    // with no value to follow the key there is nothing to align it to, so a set's node is
    // the header plus the key and not a byte more
    const size_t val_offset = val_size == 0
        ? key_size
        : nad_align_up(key_size, alignof(max_align_t));

    size_t kv_bytes;
    if (ckd_add(&kv_bytes, val_offset, val_size)) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    nad_HMap *obj = nad_alloc(al, sizeof(nad_HMap));
    if (!obj) {
        return NAD_STATUS_ERR_NO_MEM;
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
        const nad_Status st = nad_hmap_reserve(obj, cap);
        if (NAD_STATUS_IS_ERR(st)) {
            nad_dealloc(al, obj, sizeof(nad_HMap));
            return st;
        }
    }

    ASSERT_HMAP(obj);

    *out = obj;

    return NAD_STATUS_OK;
}

void nad_hmap_drop(nad_HMap *self) {
    if (!self) {
        return;
    }

    ASSERT_HMAP(self);

    nad_Al *al_copy = self->al;
    clear_nodes(self);
    nad_dealloc(al_copy, self->buckets, self->bucket_count * sizeof(nad_HMapNode *));
    nad_dealloc(al_copy, self, sizeof(nad_HMap));
}

/* ========== copy ========== */

nad_Status nad_hmap_copy(const nad_HMap *self, nad_HMap **out) {
    ASSERT_HMAP(self);

    return nad_hmap_copy_with(self, self->al, out);
}

nad_Status nad_hmap_copy_with(const nad_HMap *self, nad_Al *al, nad_HMap **out) {
    ASSERT_HMAP(self);
    assert(al);
    assert(out);

    nad_HMap *obj;
    nad_Status st = nad_hmap_new_raw_(self->len, self->key_size, self->val_size, self->hasher, self->eq, al, &obj);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    for (const nad_HMapNode *node = first_from(self, 0); node; node = nad_hmap_node_next(self, node)) {
        const void *val = self->val_size > 0 ? node_val(self, node) : nullptr;
        st = nad_hmap_insert(obj, node_key(node), val, nullptr);
        if (NAD_STATUS_IS_ERR(st)) {
            nad_hmap_drop(obj);
            return st;
        }
    }

    *out = obj;

    return NAD_STATUS_OK;
}

nad_Status nad_hmap_copy_assign(const nad_HMap *self, nad_HMap *other) {
    ASSERT_HMAP(self);
    ASSERT_HMAP(other);
    assert(self->key_size == other->key_size);
    assert(self->val_size == other->val_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    // the whole clone is built before anything of 'other' is touched, so a refusal
    // halfway through leaves the target exactly as it was
    nad_HMap *clone;
    const nad_Status st = nad_hmap_copy_with(self, other->al, &clone);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    NAD_SWAP(*other, *clone);
    nad_hmap_drop(clone);

    ASSERT_HMAP(other);

    return NAD_STATUS_OK;
}

/* ========== info ========== */

size_t nad_hmap_len(const nad_HMap *self) {
    ASSERT_HMAP(self);

    return self->len;
}

size_t nad_hmap_bucket_count(const nad_HMap *self) {
    ASSERT_HMAP(self);

    return self->bucket_count;
}

size_t nad_hmap_key_size(const nad_HMap *self) {
    ASSERT_HMAP(self);

    return self->key_size;
}

size_t nad_hmap_val_size(const nad_HMap *self) {
    ASSERT_HMAP(self);

    return self->val_size;
}

nad_Al *nad_hmap_al(const nad_HMap *self) {
    ASSERT_HMAP(self);

    return self->al;
}

nad_Hasher nad_hmap_hasher(const nad_HMap *self) {
    ASSERT_HMAP(self);

    return self->hasher;
}

nad_Eq nad_hmap_key_eq(const nad_HMap *self) {
    ASSERT_HMAP(self);

    return self->eq;
}

/* ========== compare ========== */

bool nad_hmap_eq(const nad_HMap *a, const nad_HMap *b) {
    ASSERT_HMAP(a);
    ASSERT_HMAP(b);
    assert(a->key_size == b->key_size);
    assert(a->val_size == b->val_size);

    return eq_impl(a, b, nullptr);
}

bool nad_hmap_eq_by(const nad_HMap *a, const nad_HMap *b, nad_Eq val_eq) {
    ASSERT_HMAP(a);
    ASSERT_HMAP(b);
    assert(a->key_size == b->key_size);
    assert(a->val_size == b->val_size);
    assert(val_eq);

    return eq_impl(a, b, val_eq);
}

/* ========== lookup ========== */

const void *nad_hmap_get(const nad_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    const nad_HMapNode *node = find_node(self, key, self->hasher(key));

    return node ? node_val(self, node) : nullptr;
}

void *nad_hmap_get_mut(nad_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    nad_HMapNode *node = find_node(self, key, self->hasher(key));

    return node ? node_val_mut(self, node) : nullptr;
}

bool nad_hmap_contains(const nad_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    return find_node(self, key, self->hasher(key)) != nullptr;
}

const nad_HMapNode *nad_hmap_find(const nad_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    return find_node(self, key, self->hasher(key));
}

nad_HMapNode *nad_hmap_find_mut(nad_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    return find_node(self, key, self->hasher(key));
}

/* ========== nodes ========== */

const nad_HMapNode *nad_hmap_first_node(const nad_HMap *self) {
    ASSERT_HMAP(self);

    return first_from(self, 0);
}

nad_HMapNode *nad_hmap_first_node_mut(nad_HMap *self) {
    ASSERT_HMAP(self);

    return first_from(self, 0);
}

const nad_HMapNode *nad_hmap_node_next(const nad_HMap *self, const nad_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);

    if (node->next) {
        return node->next;
    }

    return first_from(self, bucket_of(self, node->hash) + 1);
}

nad_HMapNode *nad_hmap_node_next_mut(nad_HMap *self, nad_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);

    if (node->next) {
        return node->next;
    }

    return first_from(self, bucket_of(self, node->hash) + 1);
}

const void *nad_hmap_node_key(const nad_HMapNode *node) {
    assert(node);

    return node_key(node);
}

const void *nad_hmap_node_val(const nad_HMap *self, const nad_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);

    return node_val(self, node);
}

void *nad_hmap_node_val_mut(const nad_HMap *self, nad_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);

    return node_val_mut(self, node);
}

/* ========== mods ========== */

nad_Status nad_hmap_insert(nad_HMap *self, const void *key, const void *val, bool *out_is_new) {
    ASSERT_HMAP(self);
    assert(key);
    assert(val || self->val_size == 0); // a value pointer is wanted exactly when there is a value

    const nad_Hash hash = self->hasher(key);

    nad_HMapNode *found = find_node(self, key, hash);
    if (found) {
        if (self->val_size > 0) {
            memcpy(node_val_mut(self, found), val, self->val_size);
        }

        if (out_is_new) {
            *out_is_new = false;
        }

        return NAD_STATUS_OK;
    }

    nad_HMapNode *node;
    const nad_Status st = add_node(self, key, val, hash, &node);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    if (out_is_new) {
        *out_is_new = true;
    }

    return NAD_STATUS_OK;
}

nad_Status nad_hmap_get_or_insert(nad_HMap *self, const void *key, const void *val_if_absent,
                                  nad_HMapNode **out_node) {
    ASSERT_HMAP(self);
    assert(key);
    assert(val_if_absent || self->val_size == 0);
    assert(out_node);

    // one hash for both halves of the question, and one walk of the bucket it names
    const nad_Hash hash = self->hasher(key);

    nad_HMapNode *found = find_node(self, key, hash);
    if (found) {
        *out_node = found;
        return NAD_STATUS_OK;
    }

    return add_node(self, key, val_if_absent, hash, out_node);
}

bool nad_hmap_remove(nad_HMap *self, const void *key) {
    ASSERT_HMAP(self);
    assert(key);

    if (self->bucket_count == 0) {
        return false;
    }

    const nad_Hash hash = self->hasher(key);

    // walking the links themselves rather than the nodes: the chain is singly linked, and
    // this is what stands in for the previous node
    nad_HMapNode **link = &self->buckets[bucket_of(self, hash)];
    while (*link) {
        if ((*link)->hash == hash && self->eq(node_key(*link), key)) {
            nad_HMapNode *dead = *link;
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

void nad_hmap_remove_node(nad_HMap *self, nad_HMapNode *node) {
    ASSERT_HMAP(self);
    assert(node);
    assert(self->len > 0);

    nad_HMapNode **link = &self->buckets[bucket_of(self, node->hash)];
    while (*link && *link != node) {
        link = &(*link)->next;
    }

    assert(*link == node); // the node must belong to this map

    *link = node->next;
    node_drop(self, node);
    --self->len;

    ASSERT_HMAP(self);
}

void nad_hmap_clear(nad_HMap *self) {
    ASSERT_HMAP(self);

    clear_nodes(self);

    for (size_t i = 0; i < self->bucket_count; ++i) {
        self->buckets[i] = nullptr;
    }

    self->len = 0;

    ASSERT_HMAP(self);
}

nad_Status nad_hmap_reserve(nad_HMap *self, size_t cap) {
    ASSERT_HMAP(self);

    if (cap <= self->bucket_count) {
        return NAD_STATUS_OK;
    }

    // round_up_pow2 never returns less than the base, so a small 'cap' still gets a
    // sensible bucket array rather than one or two buckets
    const size_t want = round_up_pow2(cap);
    if (want == 0) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    return rehash(self, want);
}

nad_Status nad_hmap_shrink_to_fit(nad_HMap *self) {
    ASSERT_HMAP(self);

    if (self->len == 0) {
        // nothing left to hold: the map goes back to owning no buckets at all
        nad_dealloc(self->al, self->buckets, self->bucket_count * sizeof(nad_HMapNode *));
        self->buckets = nullptr;
        self->bucket_count = 0;

        ASSERT_HMAP(self);

        return NAD_STATUS_OK;
    }

    const size_t want = round_up_pow2(self->len);
    if (want == 0 || want >= self->bucket_count) {
        return NAD_STATUS_OK;
    }

    return rehash(self, want);
}

void nad_hmap_swap(nad_HMap *self, nad_HMap *other) {
    ASSERT_HMAP(self);
    ASSERT_HMAP(other);
    assert(self->key_size == other->key_size);
    assert(self->val_size == other->val_size);
    assert(self->al == other->al);

    if (self == other) {
        return;
    }

    NAD_SWAP(*self, *other);

    ASSERT_HMAP(self);
    ASSERT_HMAP(other);
}

/* ========== print ========== */

void nad_hmap_fprint(const nad_HMap *self, FILE *stream, nad_FPrint key_fprint, nad_FPrint val_fprint) {
    ASSERT_HMAP(self);
    assert(stream);
    assert(key_fprint);
    assert(val_fprint);

    fputc('{', stream);
    bool first = true;
    for (const nad_HMapNode *node = first_from(self, 0); node; node = nad_hmap_node_next(self, node)) {
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

void nad_hmap_print(const nad_HMap *self, nad_FPrint key_fprint, nad_FPrint val_fprint) {
    nad_hmap_fprint(self, stdout, key_fprint, val_fprint);
}

/* ========== internals ========== */

static size_t node_bytes(const nad_HMap *self) {
    return sizeof(nad_HMapNode) + self->val_offset + self->val_size;
}

static const void *node_key(const nad_HMapNode *node) {
    return node->kv;
}

static void *node_val_mut(const nad_HMap *self, nad_HMapNode *node) {
    return nad_byte_offset_mut(node->kv, 1, self->val_offset);
}

static const void *node_val(const nad_HMap *self, const nad_HMapNode *node) {
    return nad_byte_offset(node->kv, 1, self->val_offset);
}

static nad_Status node_new(const nad_HMap *self, const void *key, const void *val, nad_Hash hash, nad_HMapNode **out) {
    nad_HMapNode *node = nad_alloc(self->al, node_bytes(self));
    if (!node) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    assert(nad_ptr_is_aligned(node, alignof(max_align_t)));

    node->next = nullptr;
    node->hash = hash;
    memcpy(node->kv, key, self->key_size);
    if (self->val_size > 0) {
        memcpy(node_val_mut(self, node), val, self->val_size);
    }

    *out = node;

    return NAD_STATUS_OK;
}

static void node_drop(const nad_HMap *self, nad_HMapNode *node) {
    nad_dealloc(self->al, node, node_bytes(self));
}

static size_t bucket_of(const nad_HMap *self, nad_Hash hash) {
    assert(self->bucket_count > 0);

    return (size_t) hash & (self->bucket_count - 1);
}

static nad_HMapNode *find_node(const nad_HMap *self, const void *key, nad_Hash hash) {
    if (self->bucket_count == 0) {
        return nullptr;
    }

    for (nad_HMapNode *node = self->buckets[bucket_of(self, hash)]; node; node = node->next) {
        // the hash is compared first because it is a word: 'eq' is only asked about keys
        // that already agree on every mixed bit
        if (node->hash == hash && self->eq(node_key(node), key)) {
            return node;
        }
    }

    return nullptr;
}

static nad_Status add_node(nad_HMap *self, const void *key, const void *val, nad_Hash hash, nad_HMapNode **out) {
    // the room is taken first: growing relinks the buckets, so the one this node belongs
    // to is only known afterwards
    nad_Status st = reserve_one(self);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    nad_HMapNode *node;
    st = node_new(self, key, val, hash, &node);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    const size_t bucket = bucket_of(self, hash);
    node->next = self->buckets[bucket];
    self->buckets[bucket] = node;
    ++self->len;

    ASSERT_HMAP(self);

    *out = node;

    return NAD_STATUS_OK;
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

static nad_Status rehash(nad_HMap *self, size_t new_count) {
    assert(new_count > 0);
    assert((new_count & (new_count - 1)) == 0);

    nad_HMapNode **buckets = nad_calloc(self->al, new_count, sizeof(nad_HMapNode *));
    if (!buckets) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    for (size_t i = 0; i < self->bucket_count; ++i) {
        nad_HMapNode *node = self->buckets[i];
        while (node) {
            nad_HMapNode *next = node->next;
            const size_t bucket = (size_t) node->hash & (new_count - 1);
            node->next = buckets[bucket];
            buckets[bucket] = node;
            node = next;
        }
    }

    nad_dealloc(self->al, self->buckets, self->bucket_count * sizeof(nad_HMapNode *));
    self->buckets = buckets;
    self->bucket_count = new_count;

    ASSERT_HMAP(self);

    return NAD_STATUS_OK;
}

static nad_Status reserve_one(nad_HMap *self) {
    if (self->len < self->bucket_count) {
        return NAD_STATUS_OK;
    }

    if (self->bucket_count == 0) {
        return rehash(self, HMAP_BUCKETS_BASE);
    }

    size_t grown;
    if (ckd_mul(&grown, self->bucket_count, HMAP_GROWTH_FACTOR)) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    return rehash(self, grown);
}

static nad_HMapNode *first_from(const nad_HMap *self, size_t start) {
    for (size_t i = start; i < self->bucket_count; ++i) {
        if (self->buckets[i]) {
            return self->buckets[i];
        }
    }

    return nullptr;
}

static void clear_nodes(nad_HMap *self) {
    for (size_t i = 0; i < self->bucket_count; ++i) {
        nad_HMapNode *node = self->buckets[i];
        while (node) {
            nad_HMapNode *next = node->next;
            node_drop(self, node);
            node = next;
        }
    }
}

static bool eq_impl(const nad_HMap *a, const nad_HMap *b, nad_Eq val_eq) {
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
        for (const nad_HMapNode *node = a->buckets[i]; node; node = node->next) {
            const void *key = node_key(node);
            const nad_HMapNode *found = find_node(b, key, b->hasher(key));
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
