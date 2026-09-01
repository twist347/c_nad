#include "nad/ds/hset.h"

#include "internal/hmap_impl.h"

#include <assert.h>

/* ========== internals ========== */

#define ASSERT_HSET(s) \
    (assert(s),        \
     assert((s)->map))

// A set is a map seen through a smaller keyhole: the keys live in the map's nodes and
// every operation here is one of the map's, minus the value side. Reusing it keeps the
// chaining, the growth policy and the allocator handling in one place instead of two —
// what this type contributes is the operations it does NOT forward.
//
// The map is built through internal/hmap_impl.h with val_size 0, which is the whole point
// of that door: nothing follows the key in a node, so nothing pads it either.
struct nad_HSet {
    nad_HMap *map;
};

/// takes ownership of 'map' either way: on failure it is dropped, not handed back
[[nodiscard]]
static nad_Status wrap(nad_HMap *map, nad_HSet **out);

/* ========== lifetime ========== */

nad_Status nad_hset_new(size_t key_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al, nad_HSet **out) {
    return nad_hset_new_cap(0, key_size, hasher, eq, al, out);
}

nad_Status nad_hset_new_cap(size_t cap, size_t key_size, nad_Hasher hasher, nad_Eq eq, nad_Al *al, nad_HSet **out) {
    assert(key_size > 0);
    assert(hasher);
    assert(eq);
    assert(al);
    assert(out);

    nad_HMap *map;
    const nad_Status st = nad_hmap_new_raw_(cap, key_size, 0, hasher, eq, al, &map);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(map, out);
}

void nad_hset_drop(nad_HSet *self) {
    if (!self) {
        return;
    }

    ASSERT_HSET(self);

    nad_Al *al_copy = nad_hmap_al(self->map);
    nad_hmap_drop(self->map);
    nad_dealloc(al_copy, self, sizeof(nad_HSet));
}

/* ========== copy ========== */

nad_Status nad_hset_copy(const nad_HSet *self, nad_HSet **out) {
    ASSERT_HSET(self);
    assert(out);

    nad_HMap *map;
    const nad_Status st = nad_hmap_copy(self->map, &map);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(map, out);
}

nad_Status nad_hset_copy_assign(const nad_HSet *self, nad_HSet *other) {
    ASSERT_HSET(self);
    ASSERT_HSET(other);

    // self assignment is left to the map, which already returns early on it: a guard
    // repeated here would be a branch no test could tell from its absence
    return nad_hmap_copy_assign(self->map, other->map);
}

/* ========== info ========== */

size_t nad_hset_len(const nad_HSet *self) {
    ASSERT_HSET(self);

    return nad_hmap_len(self->map);
}

size_t nad_hset_bucket_count(const nad_HSet *self) {
    ASSERT_HSET(self);

    return nad_hmap_bucket_count(self->map);
}

size_t nad_hset_key_size(const nad_HSet *self) {
    ASSERT_HSET(self);

    return nad_hmap_key_size(self->map);
}

nad_Al *nad_hset_al(const nad_HSet *self) {
    ASSERT_HSET(self);

    return nad_hmap_al(self->map);
}

nad_Hasher nad_hset_hasher(const nad_HSet *self) {
    ASSERT_HSET(self);

    return nad_hmap_hasher(self->map);
}

nad_Eq nad_hset_key_eq(const nad_HSet *self) {
    ASSERT_HSET(self);

    return nad_hmap_key_eq(self->map);
}

/* ========== compare ========== */

bool nad_hset_eq(const nad_HSet *a, const nad_HSet *b) {
    ASSERT_HSET(a);
    ASSERT_HSET(b);
    assert(nad_hmap_key_size(a->map) == nad_hmap_key_size(b->map));

    if (a == b) {
        return true;
    }

    if (nad_hmap_len(a->map) != nad_hmap_len(b->map)) {
        return false;
    }

    // not nad_hmap_eq: the map under a set carries val_size 0, so it has no value side to
    // compare and node_val would point one past the key
    for (const nad_HMapNode *node = nad_hmap_first_node(a->map); node;
         node = nad_hmap_node_next(a->map, node)) {
        if (!nad_hmap_contains(b->map, nad_hmap_node_key(node))) {
            return false;
        }
    }

    return true;
}

/* ========== lookup ========== */

bool nad_hset_contains(const nad_HSet *self, const void *key) {
    ASSERT_HSET(self);
    assert(key);

    return nad_hmap_contains(self->map, key);
}

const nad_HSetNode *nad_hset_find(const nad_HSet *self, const void *key) {
    ASSERT_HSET(self);
    assert(key);

    return nad_hmap_find(self->map, key);
}

nad_HSetNode *nad_hset_find_mut(nad_HSet *self, const void *key) {
    ASSERT_HSET(self);
    assert(key);

    return nad_hmap_find_mut(self->map, key);
}

/* ========== nodes ========== */

const nad_HSetNode *nad_hset_first_node(const nad_HSet *self) {
    ASSERT_HSET(self);

    return nad_hmap_first_node(self->map);
}

nad_HSetNode *nad_hset_first_node_mut(nad_HSet *self) {
    ASSERT_HSET(self);

    return nad_hmap_first_node_mut(self->map);
}

const nad_HSetNode *nad_hset_node_next(const nad_HSet *self, const nad_HSetNode *node) {
    ASSERT_HSET(self);
    assert(node);

    return nad_hmap_node_next(self->map, node);
}

nad_HSetNode *nad_hset_node_next_mut(nad_HSet *self, nad_HSetNode *node) {
    ASSERT_HSET(self);
    assert(node);

    return nad_hmap_node_next_mut(self->map, node);
}

const void *nad_hset_node_key(const nad_HSetNode *node) {
    assert(node);

    return nad_hmap_node_key(node);
}

/* ========== mods ========== */

nad_Status nad_hset_insert(nad_HSet *self, const void *key, bool *out_is_new) {
    ASSERT_HSET(self);
    assert(key);

    // no value to hand over: the map's val_size is 0, so it asks for none
    return nad_hmap_insert(self->map, key, nullptr, out_is_new);
}

bool nad_hset_remove(nad_HSet *self, const void *key) {
    ASSERT_HSET(self);
    assert(key);

    return nad_hmap_remove(self->map, key);
}

void nad_hset_remove_node(nad_HSet *self, nad_HSetNode *node) {
    ASSERT_HSET(self);
    assert(node);

    nad_hmap_remove_node(self->map, node);
}

void nad_hset_clear(nad_HSet *self) {
    ASSERT_HSET(self);

    nad_hmap_clear(self->map);
}

nad_Status nad_hset_reserve(nad_HSet *self, size_t cap) {
    ASSERT_HSET(self);

    return nad_hmap_reserve(self->map, cap);
}

nad_Status nad_hset_shrink_to_fit(nad_HSet *self) {
    ASSERT_HSET(self);

    return nad_hmap_shrink_to_fit(self->map);
}

void nad_hset_swap(nad_HSet *self, nad_HSet *other) {
    ASSERT_HSET(self);
    ASSERT_HSET(other);

    nad_hmap_swap(self->map, other->map);
}

/* ========== print ========== */

void nad_hset_fprint(const nad_HSet *self, FILE *stream, nad_FPrint fprint) {
    ASSERT_HSET(self);
    assert(stream);
    assert(fprint);

    fputc('{', stream);
    bool first = true;
    for (const nad_HSetNode *node = nad_hset_first_node(self); node; node = nad_hset_node_next(self, node)) {
        if (!first) {
            fputs(", ", stream);
        }
        first = false;
        fprint(stream, nad_hset_node_key(node));
    }
    fputs("}\n", stream);
}

void nad_hset_print(const nad_HSet *self, nad_FPrint fprint) {
    nad_hset_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

static nad_Status wrap(nad_HMap *map, nad_HSet **out) {
    assert(map);
    assert(out);

    nad_HSet *obj = nad_alloc(nad_hmap_al(map), sizeof(nad_HSet));
    if (!obj) {
        nad_hmap_drop(map);
        return NAD_STATUS_ERR_NO_MEM;
    }

    obj->map = map;

    *out = obj;

    return NAD_STATUS_OK;
}
