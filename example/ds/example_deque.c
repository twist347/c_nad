// for @snippet

#include "tda/algo/sort.h"
#include "tda/alloc/default.h"
#include "tda/core/cmp.h"
#include "tda/core/print.h"
#include "tda/core/span.h"
#include "tda/ds/deque.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the elems land front to back, in a ring that starts out unwrapped
    tda_Al *al = tda_al_default();

    tda_Deque *d = nullptr;
    if (TDA_STATUS_IS_ERR(TDA_DEQUE_OF(int32_t, al, &d, 3, 4, 5))) {
        return 1;
    }

    tda_deque_print(d, tda_fprint_i32); // [3, 4, 5]
    /// [build]

    /// [ends]
    // both ends cost O(1) amortized — that is the whole point of the ring, and what a vec
    // cannot do at the front. Every push may grow the block, so every push has a status
    int rc = 1;

    if (TDA_STATUS_IS_ERR(TDA_DEQUE_PUSH_FRONT(int32_t, d, 2))) {
        goto out;
    }
    if (TDA_STATUS_IS_ERR(TDA_DEQUE_PUSH_BACK(int32_t, d, 6))) {
        goto out;
    }
    tda_deque_print(d, tda_fprint_i32); // [2, 3, 4, 5, 6]

    printf(
        "%" PRId32 " .. %" PRId32 "\n", *TDA_DEQUE_FRONT_AS(int32_t, d),
        *TDA_DEQUE_BACK_AS(int32_t, d)
    ); // 2 .. 6

    // the pops take from either end and leave the room alone
    tda_deque_pop_front(d);
    tda_deque_pop_back(d);
    tda_deque_print(d, tda_fprint_i32); // [3, 4, 5]
    /// [ends]

    /// [index]
    // an index counts from the front, so 0 is the front elem wherever the ring starts.
    // Where that is the deque never says: the capacity tells how much room there is and
    // nothing about the seam
    printf("%" PRId32 " of %zu in room for %zu\n", *TDA_DEQUE_GET_AS(int32_t, d, 0),
           tda_deque_len(d), tda_deque_cap(d)); // 3 of 3 in room for 6

    TDA_DEQUE_SET(int32_t, d, 1, 9);
    tda_deque_print(d, tda_fprint_i32); // [3, 9, 5]

    // insert and remove are the operations this type is NOT for: they shift whichever
    // side is shorter, which is half a vec's constant and still O(n)
    if (TDA_STATUS_IS_ERR(TDA_DEQUE_INSERT(int32_t, d, 1, 7))) {
        goto out;
    }
    tda_deque_print(d, tda_fprint_i32); // [3, 7, 9, 5]

    tda_deque_remove(d, 1);
    tda_deque_print(d, tda_fprint_i32); // [3, 9, 5]

    // the walk goes by index too, so the seam is never visible — and nothing is copied,
    // which a to_span would have had to do
    TDA_DEQUE_FOR_EACH_AS (int32_t, elem, d) {
        printf("%" PRId32 " ", *elem); // 3 9 5
    }
    putchar('\n');
    /// [index]

    /// [algo]
    // the contents may wrap, so there is no view to hand over: the bridge to algo is a
    // pair of copies. Out into a buffer of exactly the right length, sort it, and back
    int32_t buf[8];
    const tda_SpanMut s = TDA_SPAN_FROM_DATA_MUT(int32_t, buf, tda_deque_len(d));

    tda_deque_copy_to_span(d, s);
    tda_span_sort(s, tda_cmp_i32);
    tda_deque_copy_from_span(d, tda_span_mut_to_span(s));

    tda_deque_print(d, tda_fprint_i32); // [3, 5, 9]

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    tda_deque_drop(d);
    return rc;
    /// [algo]
}
