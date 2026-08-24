// for @snippet

#include "nad/algo/merge.h"
#include "nad/algo/sort.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/core/span.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [merge]
    const nad_Span a = NAD_SPAN_OF(int32_t, 1, 4, 7);
    const nad_Span b = NAD_SPAN_OF(int32_t, 2, 3, 8);

    int32_t buf[6];
    const nad_SpanMut dst = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    nad_span_merge(dst, a, b, nad_cmp_i32);
    nad_span_mut_print(dst, nad_fprint_i32); // [1, 2, 3, 4, 7, 8]

    // in place: a span that is two sorted runs end to end becomes one. Finding where the
    // second run starts is what nad_span_is_sorted_until is for
    const nad_SpanMut s = NAD_SPAN_OF_MUT(int32_t, 1, 4, 7, 2, 3, 8);
    const size_t mid = nad_span_is_sorted_until(nad_span_mut_to_span(s), nad_cmp_i32);

    // the allocator is a speed knob, not a requirement: null takes the buffer-free path
    nad_span_inplace_merge(s, mid, nad_cmp_i32, nad_al_default());
    nad_span_mut_print(s, nad_fprint_i32); // [1, 2, 3, 4, 7, 8]
    /// [merge]

    return 0;
}
