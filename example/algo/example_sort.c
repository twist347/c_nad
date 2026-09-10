// for @snippet

#include "trs/algo/sort.h"
#include "trs/alloc/default.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"
#include "trs/core/span.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [sort]
    const trs_SpanMut s = TRS_SPAN_OF_MUT(int32_t, 5, 3, 1, 4, 2);

    // the default: in place, allocating nothing, and not stable
    trs_span_sort(s, trs_cmp_i32);
    trs_span_mut_print(s, trs_fprint_i32); // [1, 2, 3, 4, 5]

    // stability costs a buffer, so the name that gives it takes an allocator and can fail
    const trs_SpanMut t = TRS_SPAN_OF_MUT(int32_t, 9, 7, 8);
    if (TRS_STATUS_IS_ERR(trs_span_sort_stable(t, trs_cmp_i32, trs_al_default()))) {
        return 1;
    }
    trs_span_mut_print(t, trs_fprint_i32); // [7, 8, 9]
    /// [sort]

    /// [partial]
    const trs_SpanMut u = TRS_SPAN_OF_MUT(int32_t, 5, 3, 8, 1, 9, 4, 2, 7, 6);

    // only the three smallest are asked for, and only they come out in order
    trs_span_partial_sort(u, 3, trs_cmp_i32);
    trs_span_mut_print(u, trs_fprint_i32); // [1, 2, 3, 4, 5, 9, 7, 6, 8]

    // asking instead of doing: the prefix is sorted, the span is not
    printf(
        "%s, in order up to %zu\n",
        trs_span_is_sorted(trs_span_mut_to_span(u), trs_cmp_i32) ? "sorted" : "not sorted",
        trs_span_is_sorted_until(trs_span_mut_to_span(u), trs_cmp_i32)
    ); // not sorted, in order up to 6
    /// [partial]

    return 0;
}
