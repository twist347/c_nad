// for @snippet

#include "nad/algo/sort.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/core/span.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [sort]
    const nad_SpanMut s = NAD_SPAN_OF_MUT(int32_t, 5, 3, 1, 4, 2);

    // the default: in place, allocating nothing, and not stable
    nad_span_sort(s, nad_cmp_i32);
    nad_span_mut_print(s, nad_fprint_i32); // [1, 2, 3, 4, 5]

    // stability costs a buffer, so the name that gives it takes an allocator and can fail
    const nad_SpanMut t = NAD_SPAN_OF_MUT(int32_t, 9, 7, 8);
    if (NAD_STATUS_IS_ERR(nad_span_sort_stable(t, nad_cmp_i32, nad_al_default()))) {
        return 1;
    }
    nad_span_mut_print(t, nad_fprint_i32); // [7, 8, 9]
    /// [sort]

    /// [partial]
    const nad_SpanMut u = NAD_SPAN_OF_MUT(int32_t, 5, 3, 8, 1, 9, 4, 2, 7, 6);

    // only the three smallest are asked for, and only they come out in order
    nad_span_partial_sort(u, 3, nad_cmp_i32);
    nad_span_mut_print(u, nad_fprint_i32); // [1, 2, 3, 4, 5, 9, 7, 6, 8]

    // asking instead of doing: the prefix is sorted, the span is not
    printf(
        "%s, in order up to %zu\n",
        nad_span_is_sorted(nad_span_mut_to_span(u), nad_cmp_i32) ? "sorted" : "not sorted",
        nad_span_is_sorted_until(nad_span_mut_to_span(u), nad_cmp_i32)
    ); // not sorted, in order up to 6
    /// [partial]

    return 0;
}
