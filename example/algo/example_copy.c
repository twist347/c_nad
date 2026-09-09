// for @snippet

#include "tda/algo/copy.h"
#include "tda/core/print.h"
#include "tda/core/span.h"
#include "tda/core/util.h"

#include <stdint.h>
#include <stdio.h>

/// [pred]
static bool is_even(const void *elem, void *ctx) {
    TDA_UNUSED(ctx);

    return *(const int32_t *) elem % 2 == 0;
}

/// [pred]

int main() {
    /// [copy]
    const tda_Span src = TDA_SPAN_OF(int32_t, 1, 2, 3, 4, 5);

    int32_t buf[5];
    const tda_SpanMut dst = TDA_SPAN_FROM_DATA_MUT(int32_t, buf, 5);

    tda_span_copy(dst, src);
    tda_span_mut_print(dst, tda_fprint_i32); // [1, 2, 3, 4, 5]

    // copy_if packs what passes to the front and says how many that was; the tail of
    // 'dst' keeps whatever it held
    const size_t kept = tda_span_copy_if(dst, src, is_even, nullptr);
    printf("%zu kept\n", kept); // 2 kept
    tda_span_mut_print(tda_span_sub_mut(dst, 0, kept), tda_fprint_i32); // [2, 4]

    // the two spans overlap here, so it is copy_overlapping or nothing: plain copy reads what
    // it has already written
    const tda_SpanMut all = TDA_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5);
    tda_span_copy_overlapping(tda_span_sub_mut(all, 1, 4), tda_span_sub(tda_span_mut_to_span(all), 0, 4));
    tda_span_mut_print(all, tda_fprint_i32); // [1, 1, 2, 3, 4]
    /// [copy]

    return 0;
}
