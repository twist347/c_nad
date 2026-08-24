// for @snippet

#include "nad/algo/set.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/core/span.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [ops]
    // both sides sorted by the same cmp, and equal elems are counted, not collapsed: 'a'
    // holds two 2s and 'b' holds one
    const nad_Span a = NAD_SPAN_OF(int32_t, 1, 2, 2, 5);
    const nad_Span b = NAD_SPAN_OF(int32_t, 2, 3, 5);

    // 'dst' has to fit the worst case, and the return says how much of it was filled
    int32_t buf[7];
    const nad_SpanMut dst = NAD_SPAN_NEW_MUT(int32_t, buf, 7);

    size_t len = nad_span_set_union(dst, a, b, nad_cmp_i32);
    nad_span_mut_print(nad_span_sub_mut(dst, 0, len), nad_fprint_i32); // [1, 2, 2, 3, 5]

    len = nad_span_set_intersection(dst, a, b, nad_cmp_i32);
    nad_span_mut_print(nad_span_sub_mut(dst, 0, len), nad_fprint_i32); // [2, 5]

    len = nad_span_set_difference(dst, a, b, nad_cmp_i32);
    nad_span_mut_print(nad_span_sub_mut(dst, 0, len), nad_fprint_i32); // [1, 2] — one 2 survives

    printf("%d\n", nad_span_includes(a, NAD_SPAN_OF(int32_t, 2, 5), nad_cmp_i32)); // 1
    /// [ops]

    return 0;
}
