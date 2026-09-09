#include "terse/ds/hset.h"

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
struct trs_HSet {
    trs_HMap *map;
};

/// takes ownership of 'map' either way: on failure it is dropped, not handed back
[[nodiscard]]
static trs_Status wrap(trs_HMap *map, trs_HSet **out);

/* ========== lifetime ========== */

trs_Status trs_hset_new(size_t key_size, trs_Hasher hasher, trs_Eq eq, trs_Al *al, trs_HSet **out) {
    return trs_hset_new_cap(0, key_size, hasher, eq, al, out);
}

trs_Status trs_hset_new_cap(size_t cap, size_t key_size, trs_Hasher hasher, trs_Eq eq, trs_Al *al, trs_HSet **out) {
    assert(key_size > 0);
    assert(hasher);
    assert(eq);
    assert(al);
    assert(out);

    trs_HMap *map;
    const trs_Status st = trs_hmap_new_raw_(cap, key_size, 0, hasher, eq, al, &map);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(map, out);
}

void trs_hset_drop(trs_HSet *self) {
    if (!self) {
        return;
    }

    ASSERT_HSET(self);

    trs_Al *al_copy = trs_hmap_al(self->map);
    trs_hmap_drop(self->map);
    trs_dealloc(al_copy, self, sizeof(trs_HSet));
}

/* ========== copy ========== */

trs_Status trs_hset_copy(const trs_HSet *self, trs_HSet **out) {
    ASSERT_HSET(self);

    return trs_hset_copy_with(self, trs_hmap_al(self->map), out);
}

trs_Status trs_hset_copy_with(const trs_HSet *self, trs_Al *al, trs_HSet **out) {
    ASSERT_HSET(self);
    assert(al);
    assert(out);

    trs_HMap *map;
    const trs_Status st = trs_hmap_copy_with(self->map, al, &map);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(map, out);
}

trs_Status trs_hset_copy_assign(const trs_HSet *self, trs_HSet *other) {
    ASSERT_HSET(self);
    ASSERT_HSET(other);

    // self assignment is left to the map, which already returns early on it: a guard
    // repeated here would be a branch no test could tell from its absence
    return trs_hmap_copy_assign(self->map, other->map);
}

trs_Status trs_hset_move_assign(trs_HSet *self, trs_HSet *other) {
    ASSERT_HSET(self);
    ASSERT_HSET(other);

    // self assignment is left to the map, which already returns early on it: a guard
    // repeated here would be a branch no test could tell from its absence
    return trs_hmap_move_assign(self->map, other->map);
}

/* ========== info ========== */

size_t trs_hset_len(const trs_HSet *self) {
    ASSERT_HSET(self);

    return trs_hmap_len(self->map);
}

size_t trs_hset_bucket_count(const trs_HSet *self) {
    ASSERT_HSET(self);

    return trs_hmap_bucket_count(self->map);
}

size_t trs_hset_key_size(const trs_HSet *self) {
    ASSERT_HSET(self);

    return trs_hmap_key_size(self->map);
}

trs_Al *trs_hset_al(const trs_HSet *self) {
    ASSERT_HSET(self);

    return trs_hmap_al(self->map);
}

trs_Hasher trs_hset_hasher(const trs_HSet *self) {
    ASSERT_HSET(self);

    return trs_hmap_hasher(self->map);
}

trs_Eq trs_hset_key_eq(const trs_HSet *self) {
    ASSERT_HSET(self);

    return trs_hmap_key_eq(self->map);
}

/* ========== compare ========== */

bool trs_hset_eq(const trs_HSet *a, const trs_HSet *b) {
    ASSERT_HSET(a);
    ASSERT_HSET(b);
    assert(trs_hmap_key_size(a->map) == trs_hmap_key_size(b->map));

    if (a == b) {
        return true;
    }

    if (trs_hmap_len(a->map) != trs_hmap_len(b->map)) {
        return false;
    }

    // not trs_hmap_eq: the map under a set carries val_size 0, so it has no value side to
    // compare and node_val would point one past the key
    for (const trs_HMapNode *node = trs_hmap_first_node(a->map); node;
         node = trs_hmap_node_next(a->map, node)) {
        if (!trs_hmap_contains(b->map, trs_hmap_node_key(node))) {
            return false;
        }
    }

    return true;
}

/* ========== lookup ========== */

bool trs_hset_contains(const trs_HSet *self, const void *key) {
    ASSERT_HSET(self);
    assert(key);

    return trs_hmap_contains(self->map, key);
}

const trs_HSetNode *trs_hset_find(const trs_HSet *self, const void *key) {
    ASSERT_HSET(self);
    assert(key);

    return trs_hmap_find(self->map, key);
}

trs_HSetNode *trs_hset_find_mut(trs_HSet *self, const void *key) {
    ASSERT_HSET(self);
    assert(key);

    return trs_hmap_find_mut(self->map, key);
}

/* ========== nodes ========== */

const trs_HSetNode *trs_hset_first_node(const trs_HSet *self) {
    ASSERT_HSET(self);

    return trs_hmap_first_node(self->map);
}

trs_HSetNode *trs_hset_first_node_mut(trs_HSet *self) {
    ASSERT_HSET(self);

    return trs_hmap_first_node_mut(self->map);
}

const trs_HSetNode *trs_hset_node_next(const trs_HSet *self, const trs_HSetNode *node) {
    ASSERT_HSET(self);
    assert(node);

    return trs_hmap_node_next(self->map, node);
}

trs_HSetNode *trs_hset_node_next_mut(trs_HSet *self, trs_HSetNode *node) {
    ASSERT_HSET(self);
    assert(node);

    return trs_hmap_node_next_mut(self->map, node);
}

const void *trs_hset_node_key(const trs_HSetNode *node) {
    assert(node);

    return trs_hmap_node_key(node);
}

/* ========== mods ========== */

trs_Status trs_hset_insert(trs_HSet *self, const void *key, bool *out_is_new) {
    ASSERT_HSET(self);
    assert(key);

    // no value to hand over: the map's val_size is 0, so it asks for none
    return trs_hmap_insert(self->map, key, nullptr, out_is_new);
}

bool trs_hset_remove(trs_HSet *self, const void *key) {
    ASSERT_HSET(self);
    assert(key);

    return trs_hmap_remove(self->map, key);
}

void trs_hset_remove_node(trs_HSet *self, trs_HSetNode *node) {
    ASSERT_HSET(self);
    assert(node);

    trs_hmap_remove_node(self->map, node);
}

void trs_hset_clear(trs_HSet *self) {
    ASSERT_HSET(self);

    trs_hmap_clear(self->map);
}

trs_Status trs_hset_reserve(trs_HSet *self, size_t cap) {
    ASSERT_HSET(self);

    return trs_hmap_reserve(self->map, cap);
}

trs_Status trs_hset_shrink_to_fit(trs_HSet *self) {
    ASSERT_HSET(self);

    return trs_hmap_shrink_to_fit(self->map);
}

void trs_hset_swap(trs_HSet *self, trs_HSet *other) {
    ASSERT_HSET(self);
    ASSERT_HSET(other);

    trs_hmap_swap(self->map, other->map);
}

/* ========== print ========== */

void trs_hset_fprint(const trs_HSet *self, FILE *stream, trs_FPrint fprint) {
    ASSERT_HSET(self);
    assert(stream);
    assert(fprint);

    fputc('{', stream);
    bool first = true;
    for (const trs_HSetNode *node = trs_hset_first_node(self); node; node = trs_hset_node_next(self, node)) {
        if (!first) {
            fputs(", ", stream);
        }
        first = false;
        fprint(stream, trs_hset_node_key(node));
    }
    fputs("}\n", stream);
}

void trs_hset_print(const trs_HSet *self, trs_FPrint fprint) {
    trs_hset_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

static trs_Status wrap(trs_HMap *map, trs_HSet **out) {
    assert(map);
    assert(out);

    trs_HSet *obj = trs_alloc(trs_hmap_al(map), sizeof(trs_HSet));
    if (!obj) {
        trs_hmap_drop(map);
        return TRS_STATUS_ERR_NO_MEM;
    }

    obj->map = map;

    *out = obj;

    return TRS_STATUS_OK;
}
