// for @snippet

#include "trs/algo/merge.h"
#include "trs/algo/sort.h"
#include "trs/alloc/default.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"
#include "trs/core/span.h"

#include <stdint.h>

int main() {
    /// [merge]
    const trs_Span a = TRS_SPAN_OF(int32_t, 1, 4, 7);
    const trs_Span b = TRS_SPAN_OF(int32_t, 2, 3, 8);

    int32_t buf[6];
    const trs_SpanMut dst = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 6);

    trs_span_merge(dst, a, b, trs_cmp_i32);
    trs_span_mut_print(dst, trs_fprint_i32); // [1, 2, 3, 4, 7, 8]

    // in place: a span that is two sorted runs end to end becomes one. Finding where the
    // second run starts is what trs_span_is_sorted_until is for
    const trs_SpanMut s = TRS_SPAN_OF_MUT(int32_t, 1, 4, 7, 2, 3, 8);
    const size_t mid = trs_span_is_sorted_until(trs_span_mut_to_span(s), trs_cmp_i32);

    // the allocator is a speed knob, not a requirement: null takes the buffer-free path
    trs_span_inplace_merge(s, mid, trs_cmp_i32, trs_al_default());
    trs_span_mut_print(s, trs_fprint_i32); // [1, 2, 3, 4, 7, 8]
    /// [merge]

    return 0;
}
