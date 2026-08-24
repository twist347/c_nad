// for @snippet

#include "nad/algo/copy.h"
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
    /// [copy]
    const nad_Span src = NAD_SPAN_OF(int32_t, 1, 2, 3, 4, 5);

    int32_t buf[5];
    const nad_SpanMut dst = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_copy(dst, src);
    nad_span_mut_print(dst, nad_fprint_i32); // [1, 2, 3, 4, 5]

    // copy_if packs what passes to the front and says how many that was; the tail of
    // 'dst' keeps whatever it held
    const size_t kept = nad_span_copy_if(dst, src, is_even, nullptr);
    printf("%zu kept\n", kept); // 2 kept
    nad_span_mut_print(nad_span_sub_mut(dst, 0, kept), nad_fprint_i32); // [2, 4]

    // the two spans overlap here, so it is copy_within or nothing: plain copy reads what
    // it has already written
    const nad_SpanMut all = NAD_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5);
    nad_span_copy_within(
        nad_span_sub_mut(all, 1, 4),
        nad_span_sub(nad_span_mut_to_span(all), 0, 4)
    );
    nad_span_mut_print(all, nad_fprint_i32); // [1, 1, 2, 3, 4]
    /// [copy]

    return 0;
}
