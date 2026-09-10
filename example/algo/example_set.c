// for @snippet

#include "trs/algo/set.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"
#include "trs/core/span.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [ops]
    // both sides sorted by the same cmp, and equal elems are counted, not collapsed: 'a'
    // holds two 2s and 'b' holds one
    const trs_Span a = TRS_SPAN_OF(int32_t, 1, 2, 2, 5);
    const trs_Span b = TRS_SPAN_OF(int32_t, 2, 3, 5);

    // 'dst' has to fit the worst case, and the return says how much of it was filled
    int32_t buf[7];
    const trs_SpanMut dst = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 7);

    size_t len = trs_span_set_union(dst, a, b, trs_cmp_i32);
    trs_span_mut_print(trs_span_sub_mut(dst, 0, len), trs_fprint_i32); // [1, 2, 2, 3, 5]

    len = trs_span_set_intersection(dst, a, b, trs_cmp_i32);
    trs_span_mut_print(trs_span_sub_mut(dst, 0, len), trs_fprint_i32); // [2, 5]

    len = trs_span_set_difference(dst, a, b, trs_cmp_i32);
    trs_span_mut_print(trs_span_sub_mut(dst, 0, len), trs_fprint_i32); // [1, 2] — one 2 survives

    printf("%d\n", trs_span_includes(a, TRS_SPAN_OF(int32_t, 2, 5), trs_cmp_i32)); // 1
    /// [ops]

    return 0;
}
