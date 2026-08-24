// for @snippet

#include "nad/algo/heap.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/core/span.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    int32_t buf[6] = {5, 3, 1, 4, 2};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    // a max-heap: the largest is at 0, and nothing else is promised about the order
    nad_span_make_heap(s, nad_cmp_i32);
    printf("largest is %" PRId32 "\n", *NAD_SPAN_GET_MUT_AS(int32_t, s, 0)); // 5

    // a min-heap is the same call with a descending comparator, not a second set of names
    /// [build]

    /// [push]
    // push: write the elem at the end, then sift it up over the span that now includes it
    buf[5] = 9;
    const nad_SpanMut grown = NAD_SPAN_NEW_MUT(int32_t, buf, 6);
    nad_span_push_heap(grown, nad_cmp_i32);
    printf("largest is %" PRId32 "\n", *NAD_SPAN_GET_MUT_AS(int32_t, grown, 0)); // 9

    // pop: the largest goes to the back, and the heap closes over what is left
    nad_span_pop_heap(grown, nad_cmp_i32);
    printf(
        "popped %" PRId32 ", %s\n", buf[5],
        nad_span_is_heap(nad_span_sub(nad_span_mut_to_span(grown), 0, 5), nad_cmp_i32)
            ? "the rest is still a heap"
            : "broken"
    ); // popped 9, the rest is still a heap

    // heapsort needs no name of its own
    nad_span_sort_heap(s, nad_cmp_i32);
    nad_span_mut_print(s, nad_fprint_i32); // [1, 2, 3, 4, 5]
    /// [push]

    return 0;
}
