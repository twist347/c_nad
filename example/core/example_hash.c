// for @snippet

#include "nad/alloc/default.h"
#include "nad/core/hash.h"
#include "nad/ds/hset.h"

#include <stdint.h>
#include <stdio.h>

/// [struct]
// a hasher and an equality travel as a pair, and the pair has one law: what the equality
// calls equal must hash alike. Build both out of the fields', never out of the bytes —
// nad_hash_bytes would read the padding between them
typedef struct {
    int32_t x;
    int32_t y;
} Point;

static nad_Hash hash_point(const void *v) {
    const Point *p = v;

    return nad_hash_combine(nad_hash_i32(&p->x), nad_hash_i32(&p->y));
}

static bool eq_point(const void *lhs, const void *rhs) {
    const Point *a = lhs;
    const Point *b = rhs;

    return a->x == b->x && a->y == b->y;
}
/// [struct]

int main() {
    /// [use]
    nad_HSet *seen = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_HSET_NEW(Point, hash_point, eq_point, nad_al_default(), &seen))) {
        return 1;
    }

    bool is_new = false;
    nad_Status st = nad_hset_insert(seen, &(Point){ 1, 2 }, &is_new);
    if (NAD_STATUS_IS_ERR(st)) {
        goto out;
    }
    printf("%s\n", is_new ? "new" : "seen"); // new

    // an equal point hashes alike, so the set finds it and does not store it twice
    st = nad_hset_insert(seen, &(Point){ 1, 2 }, &is_new);
    if (NAD_STATUS_IS_ERR(st)) {
        goto out;
    }
    printf("%s, %zu in all\n", is_new ? "new" : "seen", nad_hset_len(seen)); // seen, 1 in all

out:
    nad_hset_drop(seen);
    return NAD_STATUS_IS_ERR(st) ? 1 : 0;
    /// [use]
}
