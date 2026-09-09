// for @snippet

#include "terse/algo/copy.h"
#include "terse/core/print.h"
#include "terse/core/span.h"
#include "terse/core/util.h"

#include <stdint.h>
#include <stdio.h>

/// [pred]
static bool is_even(const void *elem, void *ctx) {
    TRS_UNUSED(ctx);

    return *(const int32_t *) elem % 2 == 0;
}

/// [pred]

int main() {
    /// [copy]
    const trs_Span src = TRS_SPAN_OF(int32_t, 1, 2, 3, 4, 5);

    int32_t buf[5];
    const trs_SpanMut dst = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 5);

    trs_span_copy(dst, src);
    trs_span_mut_print(dst, trs_fprint_i32); // [1, 2, 3, 4, 5]

    // copy_if packs what passes to the front and says how many that was; the tail of
    // 'dst' keeps whatever it held
    const size_t kept = trs_span_copy_if(dst, src, is_even, nullptr);
    printf("%zu kept\n", kept); // 2 kept
    trs_span_mut_print(trs_span_sub_mut(dst, 0, kept), trs_fprint_i32); // [2, 4]

    // the two spans overlap here, so it is copy_overlapping or nothing: plain copy reads what
    // it has already written
    const trs_SpanMut all = TRS_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5);
    trs_span_copy_overlapping(trs_span_sub_mut(all, 1, 4), trs_span_sub(trs_span_mut_to_span(all), 0, 4));
    trs_span_mut_print(all, trs_fprint_i32); // [1, 1, 2, 3, 4]
    /// [copy]

    return 0;
}
