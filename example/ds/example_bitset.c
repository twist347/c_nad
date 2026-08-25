// for @snippet

#include "nad/alloc/default.h"
#include "nad/ds/bitset.h"

#include <stdio.h>

int main() {
    /// [build]
    // the universe is named once and never grows: this set holds a subset of 0 .. 11
    nad_Al *al = nad_al_default();

    nad_BitSet *a = nullptr;
    if (NAD_STATUS_IS_ERR(nad_bitset_new(12, al, &a))) {
        return 1;
    }

    nad_bitset_set(a, 0);
    nad_bitset_set(a, 3);
    nad_bitset_set(a, 7);

    // the printer shows the members, not a row of bits
    nad_bitset_print(a); // {0, 3, 7}
    printf("%zu of %zu\n", nad_bitset_count(a), nad_bitset_len(a)); // 3 of 12
    /// [build]

    /// [bits]
    // one index at a time. There is no elem to hand in or out — an index is its own key,
    // and one outside the universe asserts rather than returning a status
    printf("%d %d\n", nad_bitset_test(a, 3), nad_bitset_test(a, 4)); // 1 0

    nad_bitset_flip(a, 3); // in, so now out
    nad_bitset_assign(a, 4, true); // unlike flip, asking twice changes nothing
    nad_bitset_print(a); // {0, 4, 7}

    nad_bitset_clear(a, 4);
    /// [bits]

    /// [scan]
    // find_next takes where to start, so a walk of the members needs no bound of its own:
    // a start past the universe is a miss and not an assert
    size_t idx;
    for (size_t from = 0; nad_bitset_find_next(a, from, &idx); from = idx + 1) {
        printf("%zu ", idx); // 0 7
    }
    putchar('\n');

    // and the same for what is not in it
    if (nad_bitset_find_next_clear(a, 0, &idx)) {
        printf("first gap at %zu\n", idx); // first gap at 1
    }
    /// [scan]

    /// [set ops]
    // the pairwise ops write into the left operand and only read the right, and both must
    // name the same universe — a bitset over another one is a different kind of set
    nad_BitSet *b = nullptr;
    nad_BitSet *both = nullptr;
    int rc = 1;

    if (NAD_STATUS_IS_ERR(nad_bitset_new(12, al, &b))) {
        goto out;
    }
    nad_bitset_set(b, 7);
    nad_bitset_set(b, 9);

    printf("%d %d\n", nad_bitset_intersects(a, b), nad_bitset_is_subset(b, a)); // 1 0

    // union and difference need somewhere to land, so they work on a copy of the left
    if (NAD_STATUS_IS_ERR(nad_bitset_copy(a, &both))) {
        goto out;
    }

    nad_bitset_union(both, b);
    nad_bitset_print(both); // {0, 7, 9}

    nad_bitset_difference(both, b);
    nad_bitset_print(both); // {0}

    // the complement is taken over the universe, so it holds the other eleven indices and
    // nothing above them
    nad_bitset_flip_all(both);
    printf("%zu\n", nad_bitset_count(both)); // 11

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    nad_bitset_drop(both);
    nad_bitset_drop(b);
    nad_bitset_drop(a);
    return rc;
    /// [set ops]
}
