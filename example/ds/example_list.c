// for @snippet

#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/ds/list.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // a node per elem, so there is no block and no capacity — every push is one
    // allocation, and every one of them can fail
    nad_Al *al = nad_al_default();

    nad_List *l = nullptr;
    nad_List *other = nullptr; // used further down, and named here so the exit can see it
    int rc = 1;

    if (NAD_STATUS_IS_ERR(NAD_LIST_OF(int32_t, al, &l, 3, 1, 2))) {
        return 1;
    }

    if (NAD_STATUS_IS_ERR(NAD_LIST_PUSH_FRONT(int32_t, l, 4))) {
        goto out;
    }
    if (NAD_STATUS_IS_ERR(NAD_LIST_PUSH_BACK(int32_t, l, 5))) {
        goto out;
    }
    nad_list_print(l, nad_fprint_i32); // [4, 3, 1, 2, 5]
    /// [build]

    /// [walk]
    // there is no index: a walk is how an elem is reached, and null is what ends it
    NAD_LIST_FOR_EACH (node, l) {
        printf("%" PRId32 " ", *NAD_LIST_NODE_ELEM_AS(int32_t, node));
    }
    putchar('\n'); // 4 3 1 2 5

    // and back the other way, which is what the second pointer in a node buys
    for (const nad_ListNode *node = nad_list_last_node(l); node; node = nad_list_node_prev(node)) {
        printf("%" PRId32 " ", *NAD_LIST_NODE_ELEM_AS(int32_t, node));
    }
    putchar('\n'); // 5 2 1 3 4
    /// [walk]

    /// [positions]
    // a position is a node, and a node never moves: this one keeps naming its elem
    // through everything that follows
    nad_ListNode *at = nad_list_node_next_mut(nad_list_first_node_mut(l)); // the 3

    // with a position in hand, inserting and removing cost nothing; the walk to it is
    // what the caller pays for
    if (NAD_STATUS_IS_ERR(NAD_LIST_INSERT_BEFORE(int32_t, l, at, 9))) {
        goto out;
    }
    if (NAD_STATUS_IS_ERR(NAD_LIST_INSERT_AFTER(int32_t, l, at, 8))) {
        goto out;
    }
    nad_list_print(l, nad_fprint_i32); // [4, 9, 3, 8, 1, 2, 5]

    // removing another elem does not disturb this one
    nad_list_pop_front(l);
    nad_list_remove(l, nad_list_first_node_mut(l));
    nad_list_print(l, nad_fprint_i32); // [3, 8, 1, 2, 5]
    printf("%" PRId32 "\n", *NAD_LIST_NODE_ELEM_AS(int32_t, at)); // 3
    /// [positions]

    /// [relink]
    // sort moves the NODES and leaves the elems where they are, so 'at' still names the
    // same 3 afterwards. That is what sorting a copy could not give, and why a list sorts
    // itself instead of handing the work to algo/sort
    nad_list_sort(l, nad_cmp_i32);
    nad_list_print(l, nad_fprint_i32); // [1, 2, 3, 5, 8]
    printf("%" PRId32 "\n", *NAD_LIST_NODE_ELEM_AS(int32_t, at)); // 3

    // merging two sorted lists relinks as well: O(n + m), no allocation, and 'src' is
    // left empty rather than dropped
    if (NAD_STATUS_IS_ERR(NAD_LIST_OF(int32_t, al, &other, 0, 4, 9))) {
        goto out;
    }
    if (NAD_STATUS_IS_ERR(nad_list_merge(l, other, nad_cmp_i32))) {
        goto out;
    }
    nad_list_print(l, nad_fprint_i32); // [0, 1, 2, 3, 4, 5, 8, 9]
    printf("%zu\n", nad_list_len(other)); // 0

    nad_list_reverse(l);
    nad_list_print(l, nad_fprint_i32); // [9, 8, 5, 4, 3, 2, 1, 0]

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    nad_list_drop(other);
    nad_list_drop(l);
    return rc;
    /// [relink]
}
