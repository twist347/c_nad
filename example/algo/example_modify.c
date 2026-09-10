// for @snippet

#include "trs/algo/modify.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"
#include "trs/core/span.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [drop]
    // a span cannot resize itself, so what drops elems packs the kept ones to the front
    // and returns the new length; the tail is left in no defined state
    const trs_SpanMut s = TRS_SPAN_OF_MUT(int32_t, 1, 1, 2, 2, 2, 3);

    const size_t kept = trs_span_unique(s, trs_eq_i32);
    printf("%zu kept, ", kept);
    trs_span_mut_print(trs_span_sub_mut(s, 0, kept), trs_fprint_i32); // 3 kept, [1, 2, 3]

    // only adjacent elems are compared, so this leaves a set only over a sorted span

    const size_t left = trs_span_remove(trs_span_sub_mut(s, 0, kept), &(int32_t){2}, trs_eq_i32);
    printf("%zu left, ", left);
    trs_span_mut_print(trs_span_sub_mut(s, 0, left), trs_fprint_i32); // 2 left, [1, 3]

    // replace changes no length, so it returns nothing
    const trs_SpanMut t = TRS_SPAN_OF_MUT(int32_t, 1, 2, 1, 3);
    trs_span_replace(t, &(int32_t){1}, &(int32_t){9}, trs_eq_i32);
    trs_span_mut_print(t, trs_fprint_i32); // [9, 2, 9, 3]
    /// [drop]

    return 0;
}
