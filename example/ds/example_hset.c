// for @snippet

#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/hash.h"
#include "nad/core/print.h"
#include "nad/ds/hset.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the hasher and the equality are fixed here and travel with the keys from now on.
    // They must agree: keys equal under 'eq' have to hash alike
    nad_Al *al = nad_al_default();

    nad_HSet *s = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, al, &s))) {
        return 1;
    }

    int rc = 1;
    for (int32_t i = 1; i <= 4; ++i) {
        if (NAD_STATUS_IS_ERR(NAD_HSET_INSERT(int32_t, s, i * 10, nullptr))) {
            goto out;
        }
    }
    printf("%zu keys over %zu buckets\n", nad_hset_len(s), nad_hset_bucket_count(s)); // 4 keys over 8 buckets
    /// [build]

    /// [membership]
    // a key is either in or out — there is no value to get, to set or to hand out, and
    // that narrowing is the whole difference from ds/hmap
    printf("%d %d\n", NAD_HSET_CONTAINS(int32_t, s, 20), NAD_HSET_CONTAINS(int32_t, s, 25)); // 1 0

    // insert says whether the key was new, which is how a set reports a duplicate: the
    // one already there is left as it is
    bool is_new;
    if (NAD_STATUS_IS_ERR(NAD_HSET_INSERT(int32_t, s, 20, &is_new))) {
        goto out;
    }
    printf("%d %zu\n", is_new, nad_hset_len(s)); // 0 4

    // remove says whether it was there, and cannot fail — nothing is allocated. The two
    // calls are separate statements on purpose: as arguments of one printf their order
    // would be unspecified, and each changes what the other sees
    const bool was_there = NAD_HSET_REMOVE(int32_t, s, 20);
    const bool again = NAD_HSET_REMOVE(int32_t, s, 20);
    printf("%d %d\n", was_there, again); // 1 0
    /// [membership]

    /// [walk]
    // the walk needs the set as well as the node, because a chain ends long before the
    // buckets do. The order is unspecified: it follows the buckets
    int32_t sum = 0;
    for (const nad_HSetNode *node = nad_hset_first_node(s); node; node = nad_hset_node_next(s, node)) {
        sum += *NAD_HSET_NODE_KEY_AS(int32_t, node);
    }
    printf("%" PRId32 "\n", sum); // 80

    // two sets are equal when they hold the same keys, whatever order they went in and
    // however many buckets each ended up with
    nad_HSet *twin = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_HSET_NEW_CAP(int32_t, 64, nad_hash_i32, nad_eq_i32, al, &twin))) {
        goto out;
    }
    for (int32_t i = 4; i >= 1; --i) {
        if (i != 2 && NAD_STATUS_IS_ERR(NAD_HSET_INSERT(int32_t, twin, i * 10, nullptr))) {
            goto out_twin;
        }
    }
    printf(
        "%zu vs %zu buckets, equal: %d\n", nad_hset_bucket_count(s),
        nad_hset_bucket_count(twin), nad_hset_eq(s, twin)
    ); // 8 vs 64 buckets, equal: 1

    rc = 0;
out_twin:
    nad_hset_drop(twin);
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    nad_hset_drop(s);
    return rc;
    /// [walk]
}
