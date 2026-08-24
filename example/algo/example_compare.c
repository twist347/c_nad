// for @snippet

#include "nad/algo/compare.h"
#include "nad/core/cmp.h"
#include "nad/core/span.h"

#include <stdio.h>
#include <stdint.h>

int main() {
    /// [compare]
    const nad_Span a = NAD_SPAN_OF(int32_t, 1, 2, 3);
    const nad_Span b = NAD_SPAN_OF(int32_t, 1, 2, 4);

    printf("%d %d\n", nad_span_eq(a, b), nad_span_eq_by(a, b, nad_eq_i32)); // 0 0

    size_t idx;
    if (nad_span_mismatch(a, b, nad_eq_i32, &idx)) {
        printf("they part at %zu\n", idx); // they part at 2
    }

    // dictionary order: the first differing pair decides, and a prefix orders first
    const nad_Span shorter = NAD_SPAN_OF(int32_t, 1, 2);
    printf(
        "%d %d\n", nad_span_cmp(a, b, nad_cmp_i32),
        nad_span_cmp(shorter, a, nad_cmp_i32)
    ); // -1 -1
    /// [compare]

    return 0;
}
