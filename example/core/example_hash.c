// for @snippet

#include "terse/alloc/default.h"
#include "terse/core/hash.h"
#include "terse/ds/hset.h"

#include <stdint.h>
#include <stdio.h>

/// [struct]
// a hasher and an equality travel as a pair, and the pair has one law: what the equality
// calls equal must hash alike. Build both out of the fields', never out of the bytes —
// trs_hash_bytes would read the padding between them
typedef struct {
    int32_t x;
    int32_t y;
} Point;

static trs_Hash hash_point(const void *v) {
    const Point *p = v;

    return trs_hash_combine(trs_hash_i32(&p->x), trs_hash_i32(&p->y));
}

static bool eq_point(const void *lhs, const void *rhs) {
    const Point *a = lhs;
    const Point *b = rhs;

    return a->x == b->x && a->y == b->y;
}
/// [struct]

int main() {
    /// [use]
    trs_HSet *seen = nullptr;
    if (TRS_STATUS_IS_ERR(TRS_HSET_NEW(Point, hash_point, eq_point, trs_al_default(), &seen))) {
        return 1;
    }

    bool is_new = false;
    trs_Status st = trs_hset_insert(seen, &(Point){ 1, 2 }, &is_new);
    if (TRS_STATUS_IS_ERR(st)) {
        goto out;
    }
    printf("%s\n", is_new ? "new" : "seen"); // new

    // an equal point hashes alike, so the set finds it and does not store it twice
    st = trs_hset_insert(seen, &(Point){ 1, 2 }, &is_new);
    if (TRS_STATUS_IS_ERR(st)) {
        goto out;
    }
    printf("%s, %zu in all\n", is_new ? "new" : "seen", trs_hset_len(seen)); // seen, 1 in all

out:
    trs_hset_drop(seen);
    return TRS_STATUS_IS_ERR(st) ? 1 : 0;
    /// [use]
}
