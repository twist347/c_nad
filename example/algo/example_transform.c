// for @snippet

#include "trs/algo/transform.h"
#include "trs/core/print.h"
#include "trs/core/span.h"
#include "trs/core/util.h"

#include <stdint.h>

/// [ops]
static void widen_and_square(void *dst, const void *src, void *ctx) {
    TRS_UNUSED(ctx);

    const int64_t v = *(const int32_t *) src;
    *(int64_t *) dst = v * v;
}

static void add(void *dst, const void *a, const void *b, void *ctx) {
    TRS_UNUSED(ctx);

    *(int32_t *) dst = *(const int32_t *) a + *(const int32_t *) b;
}

/// [ops]

int main() {
    /// [map]
    const trs_Span src = TRS_SPAN_OF(int32_t, 1, 2, 3, 4);

    int64_t wide[4];
    const trs_SpanMut dst = TRS_SPAN_FROM_DATA_MUT(int64_t, wide, 4);

    // only the lengths have to match; the elem sizes are 4 and 8
    trs_span_transform(dst, src, widen_and_square, nullptr);
    trs_span_mut_print(dst, trs_fprint_i64); // [1, 4, 9, 16]

    // two sources in step
    const trs_Span other = TRS_SPAN_OF(int32_t, 10, 20, 30, 40);
    const trs_SpanMut sums = TRS_SPAN_OF_MUT(int32_t, 0, 0, 0, 0);

    trs_span_zip_with(sums, src, other, add, nullptr);
    trs_span_mut_print(sums, trs_fprint_i32); // [11, 22, 33, 44]
    /// [map]

    return 0;
}
