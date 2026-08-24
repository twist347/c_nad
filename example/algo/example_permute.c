// for @snippet

#include "nad/algo/permute.h"
#include "nad/alloc/default.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/util.h"

#include <stdint.h>
#include <stdio.h>

/// [pred]
static bool is_even(const void *elem, void *ctx) {
    NAD_UNUSED(ctx);

    return *(const int32_t *) elem % 2 == 0;
}
/// [pred]

int main() {
    /// [move]
    const nad_SpanMut s = NAD_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5);

    nad_span_reverse(s);
    nad_span_mut_print(s, nad_fprint_i32); // [5, 4, 3, 2, 1]

    // rotate left so that the elem at 'mid' becomes the first
    nad_span_rotate(s, 2);
    nad_span_mut_print(s, nad_fprint_i32); // [3, 2, 1, 5, 4]
    /// [move]

    /// [partition]
    const nad_SpanMut t = NAD_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5, 6);

    // the fast split: it swaps, so neither side keeps the order it had
    const size_t at = nad_span_partition(t, is_even, nullptr);
    printf("%zu even, ", at);
    nad_span_mut_print(t, nad_fprint_i32); // 3 even, [2, 4, 6, 1, 5, 3]

    // keeping both orders costs a buffer, so that one takes an allocator and can fail
    const nad_SpanMut u = NAD_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5, 6);
    size_t boundary;
    if (NAD_STATUS_IS_ERR(
            nad_span_partition_stable(u, is_even, nullptr, nad_al_default(), &boundary))) {
        return 1;
    }
    printf("%zu even, ", boundary);
    nad_span_mut_print(u, nad_fprint_i32); // 3 even, [2, 4, 6, 1, 3, 5]
    /// [partition]

    return 0;
}
