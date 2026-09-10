// for @snippet

#include "trs/algo/sort.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"
#include "trs/core/span.h"

#include <inttypes.h>
#include <stdio.h>

int main() {
    /// [build]
    // a view borrows: the elems live in 'nums', the span only says where and how many
    int32_t nums[] = {5, 3, 1, 4, 2};
    const trs_SpanMut s = TRS_SPAN_FROM_DATA_MUT(int32_t, nums, 5);

    // the fields are public — a view owns nothing, so it has no invariant to protect
    printf("%zu elems of %zu bytes, %zu in all\n", s.len, s.elem_size,
           trs_span_bytes(trs_span_mut_to_span(s))); // 5 elems of 4 bytes, 20 in all
    /// [build]

    /// [access]
    TRS_SPAN_SET(int32_t, s, 0, 9);
    trs_span_swap_elems(s, 0, 4);
    printf("%" PRId32 "\n", *TRS_SPAN_GET_MUT_AS(int32_t, s, 0)); // 2

    // writing through the view writes the source: this is the same elem
    printf("%" PRId32 "\n", nums[0]); // 2

    // the walk that goes with a view. The elem type has to be named: a span carries
    // sizes, not types, and this is the one place that shows
    int32_t total = 0;
    TRS_SPAN_FOR_EACH_AS (int32_t, elem, trs_span_mut_to_span(s)) {
        total += *elem;
    }
    printf("%" PRId32 "\n", total); // 19
    /// [access]

    /// [sub]
    // a subspan views the same memory, offset and shortened — nothing is copied
    const trs_SpanMut tail = trs_span_sub_mut(s, 1, 4);
    trs_span_mut_print(tail, trs_fprint_i32); // [3, 1, 4, 9]
    /// [sub]

    /// [bridge]
    // the seam: ds hands out a view, algo works in place through it
    trs_span_sort(s, trs_cmp_i32);
    trs_span_print(trs_span_mut_to_span(s), trs_fprint_i32); // [1, 2, 3, 4, 9]
    /// [bridge]

    return 0;
}
