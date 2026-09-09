// for @snippet

#include "tda/alloc/default.h"
#include "tda/core/cmp.h"
#include "tda/core/print.h"
#include "tda/ds/list.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // a node per elem, so there is no block and no capacity — every push is one
    // allocation, and every one of them can fail
    tda_Al *al = tda_al_default();

    tda_List *l = nullptr;
    tda_List *other = nullptr; // used further down, and named here so the exit can see it
    int rc = 1;

    if (TDA_STATUS_IS_ERR(TDA_LIST_OF(int32_t, al, &l, 3, 1, 2))) {
        return 1;
    }

    if (TDA_STATUS_IS_ERR(TDA_LIST_PUSH_FRONT(int32_t, l, 4))) {
        goto out;
    }
    if (TDA_STATUS_IS_ERR(TDA_LIST_PUSH_BACK(int32_t, l, 5))) {
        goto out;
    }
    tda_list_print(l, tda_fprint_i32); // [4, 3, 1, 2, 5]
    /// [build]

    /// [walk]
    // there is no index: a walk is how an elem is reached, and null is what ends it
    TDA_LIST_FOR_EACH (node, l) {
        printf("%" PRId32 " ", *TDA_LIST_NODE_ELEM_AS(int32_t, node));
    }
    putchar('\n'); // 4 3 1 2 5

    // and back the other way, which is what the second pointer in a node buys
    for (const tda_ListNode *node = tda_list_back_node(l); node; node = tda_list_node_prev(node)) {
        printf("%" PRId32 " ", *TDA_LIST_NODE_ELEM_AS(int32_t, node));
    }
    putchar('\n'); // 5 2 1 3 4
    /// [walk]

    /// [positions]
    // a position is a node, and a node never moves: this one keeps naming its elem
    // through everything that follows
    tda_ListNode *at = tda_list_node_next_mut(tda_list_front_node_mut(l)); // the 3

    // with a position in hand, inserting and removing cost nothing; the walk to it is
    // what the caller pays for
    if (TDA_STATUS_IS_ERR(TDA_LIST_INSERT_BEFORE(int32_t, l, at, 9))) {
        goto out;
    }
    if (TDA_STATUS_IS_ERR(TDA_LIST_INSERT_AFTER(int32_t, l, at, 8))) {
        goto out;
    }
    tda_list_print(l, tda_fprint_i32); // [4, 9, 3, 8, 1, 2, 5]

    // removing another elem does not disturb this one
    tda_list_pop_front(l);
    tda_list_remove(l, tda_list_front_node_mut(l));
    tda_list_print(l, tda_fprint_i32); // [3, 8, 1, 2, 5]
    printf("%" PRId32 "\n", *TDA_LIST_NODE_ELEM_AS(int32_t, at)); // 3
    /// [positions]

    /// [relink]
    // sort moves the NODES and leaves the elems where they are, so 'at' still names the
    // same 3 afterwards. That is what sorting a copy could not give, and why a list sorts
    // itself instead of handing the work to algo/sort
    tda_list_sort(l, tda_cmp_i32);
    tda_list_print(l, tda_fprint_i32); // [1, 2, 3, 5, 8]
    printf("%" PRId32 "\n", *TDA_LIST_NODE_ELEM_AS(int32_t, at)); // 3

    // merging two sorted lists relinks as well: O(n + m), no allocation, and 'src' is
    // left empty rather than dropped
    if (TDA_STATUS_IS_ERR(TDA_LIST_OF(int32_t, al, &other, 0, 4, 9))) {
        goto out;
    }
    if (TDA_STATUS_IS_ERR(tda_list_merge(l, other, tda_cmp_i32))) {
        goto out;
    }
    tda_list_print(l, tda_fprint_i32); // [0, 1, 2, 3, 4, 5, 8, 9]
    printf("%zu\n", tda_list_len(other)); // 0

    tda_list_reverse(l);
    tda_list_print(l, tda_fprint_i32); // [9, 8, 5, 4, 3, 2, 1, 0]

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    tda_list_drop(other);
    tda_list_drop(l);
    return rc;
    /// [relink]
}
