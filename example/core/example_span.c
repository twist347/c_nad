// for @snippet

#include "nad/algo/sort.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/core/span.h"

#include <inttypes.h>
#include <stdio.h>

int main() {
    /// [build]
    // a view borrows: the elems live in 'nums', the span only says where and how many
    int32_t nums[] = {5, 3, 1, 4, 2};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, nums, 5);

    // the fields are public — a view owns nothing, so it has no invariant to protect
    printf("%zu elems of %zu bytes, %zu in all\n", s.len, s.elem_size,
           nad_span_bytes(nad_span_mut_to_span(s))); // 5 elems of 4 bytes, 20 in all
    /// [build]

    /// [access]
    NAD_SPAN_SET(int32_t, s, 0, 9);
    nad_span_swap_elems(s, 0, 4);
    printf("%" PRId32 "\n", *NAD_SPAN_GET_MUT_AS(int32_t, s, 0)); // 2

    // writing through the view writes the source: this is the same elem
    printf("%" PRId32 "\n", nums[0]); // 2
    /// [access]

    /// [sub]
    // a subspan views the same memory, offset and shortened — nothing is copied
    const nad_SpanMut tail = nad_span_sub_mut(s, 1, 4);
    nad_span_mut_print(tail, nad_fprint_i32); // [3, 1, 4, 9]
    /// [sub]

    /// [bridge]
    // the seam: ds hands out a view, algo works in place through it
    nad_span_sort(s, nad_cmp_i32);
    nad_span_print(nad_span_mut_to_span(s), nad_fprint_i32); // [1, 2, 3, 4, 9]
    /// [bridge]

    return 0;
}
