// for @snippet

#include "trs/algo/heap.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"
#include "trs/core/span.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    int32_t buf[6] = {5, 3, 1, 4, 2};
    const trs_SpanMut s = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 5);

    // a max-heap: the largest is at 0, and nothing else is promised about the order
    trs_span_make_heap(s, trs_cmp_i32);
    printf("largest is %" PRId32 "\n", *TRS_SPAN_GET_MUT_AS(int32_t, s, 0)); // 5

    // a min-heap is the same call with a descending comparator, not a second set of names
    /// [build]

    /// [push]
    // push: write the elem at the end, then sift it up over the span that now includes it
    buf[5] = 9;
    const trs_SpanMut grown = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 6);
    trs_span_push_heap(grown, trs_cmp_i32);
    printf("largest is %" PRId32 "\n", *TRS_SPAN_GET_MUT_AS(int32_t, grown, 0)); // 9

    // pop: the largest goes to the back, and the heap closes over what is left
    trs_span_pop_heap(grown, trs_cmp_i32);
    printf(
        "popped %" PRId32 ", %s\n", buf[5],
        trs_span_is_heap(trs_span_sub(trs_span_mut_to_span(grown), 0, 5), trs_cmp_i32)
            ? "the rest is still a heap"
            : "broken"
    ); // popped 9, the rest is still a heap

    // heapsort needs no name of its own
    trs_span_sort_heap(s, trs_cmp_i32);
    trs_span_mut_print(s, trs_fprint_i32); // [1, 2, 3, 4, 5]
    /// [push]

    return 0;
}
