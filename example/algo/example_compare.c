// for @snippet

#include "trs/algo/compare.h"
#include "trs/core/cmp.h"
#include "trs/core/span.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [compare]
    const trs_Span a = TRS_SPAN_OF(int32_t, 1, 2, 3);
    const trs_Span b = TRS_SPAN_OF(int32_t, 1, 2, 4);

    printf("%d %d\n", trs_span_eq(a, b), trs_span_eq_by(a, b, trs_eq_i32)); // 0 0

    size_t idx;
    if (trs_span_mismatch(a, b, trs_eq_i32, &idx)) {
        printf("they part at %zu\n", idx); // they part at 2
    }

    // dictionary order: the first differing pair decides, and a prefix orders first
    const trs_Span shorter = TRS_SPAN_OF(int32_t, 1, 2);
    printf(
        "%d %d\n", trs_span_cmp(a, b, trs_cmp_i32),
        trs_span_cmp(shorter, a, trs_cmp_i32)
    ); // -1 -1
    /// [compare]

    return 0;
}
