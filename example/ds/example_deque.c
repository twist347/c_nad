// for @snippet

#include "nad/algo/sort.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/ds/deque.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the elems land front to back, in a ring that starts out unwrapped
    nad_Al *al = nad_al_default();

    nad_Deque *d = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_DEQUE_OF(int32_t, al, &d, 3, 4, 5))) {
        return 1;
    }

    nad_deque_print(d, nad_fprint_i32); // [3, 4, 5]
    /// [build]

    /// [ends]
    // both ends cost O(1) amortized — that is the whole point of the ring, and what a vec
    // cannot do at the front. Every push may grow the block, so every push has a status
    int rc = 1;

    if (NAD_STATUS_IS_ERR(NAD_DEQUE_PUSH_FRONT(int32_t, d, 2))) {
        goto out;
    }
    if (NAD_STATUS_IS_ERR(NAD_DEQUE_PUSH_BACK(int32_t, d, 6))) {
        goto out;
    }
    nad_deque_print(d, nad_fprint_i32); // [2, 3, 4, 5, 6]

    printf(
        "%" PRId32 " .. %" PRId32 "\n", *NAD_DEQUE_FIRST_AS(int32_t, d),
        *NAD_DEQUE_LAST_AS(int32_t, d)
    ); // 2 .. 6

    // the pops take from either end and leave the room alone
    nad_deque_pop_front(d);
    nad_deque_pop_back(d);
    nad_deque_print(d, nad_fprint_i32); // [3, 4, 5]
    /// [ends]

    /// [index]
    // an index counts from the front, so 0 is the front elem wherever the ring starts.
    // Where that is the deque never says: the capacity tells how much room there is and
    // nothing about the seam
    printf("%" PRId32 " of %zu in room for %zu\n", *NAD_DEQUE_GET_AS(int32_t, d, 0),
           nad_deque_len(d), nad_deque_cap(d)); // 3 of 3 in room for 6

    NAD_DEQUE_SET(int32_t, d, 1, 9);
    nad_deque_print(d, nad_fprint_i32); // [3, 9, 5]

    // insert and remove are the operations this type is NOT for: they shift whichever
    // side is shorter, which is half a vec's constant and still O(n)
    if (NAD_STATUS_IS_ERR(NAD_DEQUE_INSERT(int32_t, d, 1, 7))) {
        goto out;
    }
    nad_deque_print(d, nad_fprint_i32); // [3, 7, 9, 5]

    nad_deque_remove(d, 1);
    nad_deque_print(d, nad_fprint_i32); // [3, 9, 5]
    /// [index]

    /// [algo]
    // the contents may wrap, so there is no view to hand over: the bridge to algo is a
    // pair of copies. Out into a buffer of exactly the right length, sort it, and back
    int32_t buf[8];
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, nad_deque_len(d));

    nad_deque_copy_to_span(d, s);
    nad_span_sort(s, nad_cmp_i32);
    nad_deque_copy_from_span(d, nad_span_mut_to_span(s));

    nad_deque_print(d, nad_fprint_i32); // [3, 5, 9]

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    nad_deque_drop(d);
    return rc;
    /// [algo]
}
