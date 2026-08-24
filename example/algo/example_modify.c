// for @snippet

#include "nad/algo/modify.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/core/span.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [drop]
    // a span cannot resize itself, so what drops elems packs the kept ones to the front
    // and returns the new length; the tail is left in no defined state
    const nad_SpanMut s = NAD_SPAN_OF_MUT(int32_t, 1, 1, 2, 2, 2, 3);

    const size_t kept = nad_span_unique(s, nad_eq_i32);
    printf("%zu kept, ", kept);
    nad_span_mut_print(nad_span_sub_mut(s, 0, kept), nad_fprint_i32); // 3 kept, [1, 2, 3]

    // only adjacent elems are compared, so this leaves a set only over a sorted span

    const size_t left = nad_span_remove(nad_span_sub_mut(s, 0, kept), &(int32_t){2},
                                        nad_eq_i32);
    printf("%zu left, ", left);
    nad_span_mut_print(nad_span_sub_mut(s, 0, left), nad_fprint_i32); // 2 left, [1, 3]

    // replace changes no length, so it returns nothing
    const nad_SpanMut t = NAD_SPAN_OF_MUT(int32_t, 1, 2, 1, 3);
    nad_span_replace(t, &(int32_t){1}, &(int32_t){9}, nad_eq_i32);
    nad_span_mut_print(t, nad_fprint_i32); // [9, 2, 9, 3]
    /// [drop]

    return 0;
}
