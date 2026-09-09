// for @snippet

#include "terse/algo/fill.h"
#include "terse/core/print.h"
#include "terse/core/span.h"
#include "terse/core/util.h"

#include <stdint.h>

/// [gen]
static void squares(void *dst, size_t idx, void *ctx) {
    TRS_UNUSED(ctx);

    *(int32_t *) dst = (int32_t) (idx * idx);
}

/// [gen]

int main() {
    /// [fill]
    int32_t buf[5];
    const trs_SpanMut s = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 5);

    trs_span_fill(s, &(int32_t){7});
    trs_span_mut_print(s, trs_fprint_i32); // [7, 7, 7, 7, 7]

    trs_span_fill_zero(s);
    trs_span_mut_print(s, trs_fprint_i32); // [0, 0, 0, 0, 0]

    trs_span_generate(s, squares, nullptr);
    trs_span_mut_print(s, trs_fprint_i32); // [0, 1, 4, 9, 16]
    /// [fill]

    return 0;
}
