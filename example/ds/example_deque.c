// for @snippet

#include "terse/algo/sort.h"
#include "terse/alloc/default.h"
#include "terse/core/cmp.h"
#include "terse/core/print.h"
#include "terse/core/span.h"
#include "terse/ds/deque.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the elems land front to back, in a ring that starts out unwrapped
    trs_Al *al = trs_al_default();

    trs_Deque *d = nullptr;
    if (TRS_STATUS_IS_ERR(TRS_DEQUE_OF(int32_t, al, &d, 3, 4, 5))) {
        return 1;
    }

    trs_deque_print(d, trs_fprint_i32); // [3, 4, 5]
    /// [build]

    /// [ends]
    // both ends cost O(1) amortized — that is the whole point of the ring, and what a vec
    // cannot do at the front. Every push may grow the block, so every push has a status
    int rc = 1;

    if (TRS_STATUS_IS_ERR(TRS_DEQUE_PUSH_FRONT(int32_t, d, 2))) {
        goto out;
    }
    if (TRS_STATUS_IS_ERR(TRS_DEQUE_PUSH_BACK(int32_t, d, 6))) {
        goto out;
    }
    trs_deque_print(d, trs_fprint_i32); // [2, 3, 4, 5, 6]

    printf(
        "%" PRId32 " .. %" PRId32 "\n", *TRS_DEQUE_FRONT_AS(int32_t, d),
        *TRS_DEQUE_BACK_AS(int32_t, d)
    ); // 2 .. 6

    // the pops take from either end and leave the room alone
    trs_deque_pop_front(d);
    trs_deque_pop_back(d);
    trs_deque_print(d, trs_fprint_i32); // [3, 4, 5]
    /// [ends]

    /// [index]
    // an index counts from the front, so 0 is the front elem wherever the ring starts.
    // Where that is the deque never says: the capacity tells how much room there is and
    // nothing about the seam
    printf("%" PRId32 " of %zu in room for %zu\n", *TRS_DEQUE_GET_AS(int32_t, d, 0),
           trs_deque_len(d), trs_deque_cap(d)); // 3 of 3 in room for 6

    TRS_DEQUE_SET(int32_t, d, 1, 9);
    trs_deque_print(d, trs_fprint_i32); // [3, 9, 5]

    // insert and remove are the operations this type is NOT for: they shift whichever
    // side is shorter, which is half a vec's constant and still O(n)
    if (TRS_STATUS_IS_ERR(TRS_DEQUE_INSERT(int32_t, d, 1, 7))) {
        goto out;
    }
    trs_deque_print(d, trs_fprint_i32); // [3, 7, 9, 5]

    trs_deque_remove(d, 1);
    trs_deque_print(d, trs_fprint_i32); // [3, 9, 5]

    // the walk goes by index too, so the seam is never visible — and nothing is copied,
    // which a to_span would have had to do
    TRS_DEQUE_FOR_EACH_AS (int32_t, elem, d) {
        printf("%" PRId32 " ", *elem); // 3 9 5
    }
    putchar('\n');
    /// [index]

    /// [algo]
    // the contents may wrap, so there is no view to hand over: the bridge to algo is a
    // pair of copies. Out into a buffer of exactly the right length, sort it, and back
    int32_t buf[8];
    const trs_SpanMut s = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, trs_deque_len(d));

    trs_deque_copy_to_span(d, s);
    trs_span_sort(s, trs_cmp_i32);
    trs_deque_copy_from_span(d, trs_span_mut_to_span(s));

    trs_deque_print(d, trs_fprint_i32); // [3, 5, 9]

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    trs_deque_drop(d);
    return rc;
    /// [algo]
}
