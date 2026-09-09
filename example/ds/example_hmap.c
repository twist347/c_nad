// for @snippet

#include "terse/alloc/default.h"
#include "terse/core/cmp.h"
#include "terse/core/hash.h"
#include "terse/core/print.h"
#include "terse/ds/hmap.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the hasher and the equality are fixed here and travel with the entries from now on.
    // They must agree: keys equal under 'eq' have to hash alike
    trs_Al *al = trs_al_default();

    trs_HMap *m = nullptr;
    if (TRS_STATUS_IS_ERR(TRS_HMAP_NEW(int32_t, int32_t, trs_hash_i32, trs_eq_i32, al, &m))) {
        return 1;
    }

    int rc = 1;
    bool is_new;
    if (TRS_STATUS_IS_ERR(TRS_HMAP_INSERT(int32_t, int32_t, m, 1, 10, &is_new))) {
        goto out;
    }
    printf("%d\n", is_new); // 1

    // an insert under a key already there overwrites the value and says so
    if (TRS_STATUS_IS_ERR(TRS_HMAP_INSERT(int32_t, int32_t, m, 1, 11, &is_new))) {
        goto out;
    }
    printf("%d %" PRId32 "\n", is_new, *TRS_HMAP_GET_AS(int32_t, int32_t, m, 1)); // 0 11
    /// [build]

    /// [lookup]
    // a miss is null, not an error: it has exactly one cause, so it needs no status
    printf("%d\n", TRS_HMAP_GET_AS(int32_t, int32_t, m, 99) == nullptr); // 1

    // the value is the map's to hand over for writing; the key is not
    *TRS_HMAP_GET_MUT_AS(int32_t, int32_t, m, 1) = 12;
    printf("%" PRId32 "\n", *TRS_HMAP_GET_AS(int32_t, int32_t, m, 1)); // 12

    // a position reaches the key and the value together, and removes without hashing again
    trs_HMapNode *node = trs_hmap_find_mut(m, &(int32_t){1});
    printf(
        "%" PRId32 " -> %" PRId32 "\n", *TRS_HMAP_NODE_KEY_AS(int32_t, node),
        *TRS_HMAP_NODE_VAL_AS(int32_t, m, node)
    ); // 1 -> 12

    trs_hmap_remove_node(m, node);
    printf("%zu\n", trs_hmap_len(m)); // 0
    /// [lookup]

    /// [count]
    // the counter idiom: get_or_insert hashes once and walks the bucket once whether the
    // key is there or not, where a get followed by an insert would do both twice on every
    // key it meets for the first time
    constexpr int32_t data[] = {3, 1, 3, 3, 1};

    for (size_t i = 0; i < sizeof data / sizeof data[0]; ++i) {
        trs_HMapNode *entry;
        if (TRS_STATUS_IS_ERR(trs_hmap_get_or_insert(m, &data[i], &(int32_t){0}, &entry))) {
            goto out;
        }
        ++*TRS_HMAP_NODE_VAL_MUT_AS(int32_t, m, entry);
    }

    printf(
        "1 seen %" PRId32 ", 3 seen %" PRId32 "\n", *TRS_HMAP_GET_AS(int32_t, int32_t, m, 1),
        *TRS_HMAP_GET_AS(int32_t, int32_t, m, 3)
    ); // 1 seen 2, 3 seen 3
    /// [count]

    /// [walk]
    // the walk needs the map as well as the node, because a chain ends long before the
    // buckets do. The order is unspecified: it follows the buckets
    int32_t total = 0;
    TRS_HMAP_FOR_EACH (at, m) {
        total += *TRS_HMAP_NODE_VAL_AS(int32_t, m, at);
    }
    printf("%zu keys, %" PRId32 " counted\n", trs_hmap_len(m), total); // 2 keys, 5 counted

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    trs_hmap_drop(m);
    return rc;
    /// [walk]
}
