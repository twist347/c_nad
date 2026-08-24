// for @snippet

#include "nad/algo/transform.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/util.h"

#include <stdint.h>

/// [ops]
// the op knows both types, which is what lets the elem type change on the way: an
// int32_t is read and an int64_t is written
static void widen_and_square(void *dst, const void *src, void *ctx) {
    NAD_UNUSED(ctx);

    const int64_t v = *(const int32_t *) src;
    *(int64_t *) dst = v * v;
}

static void add(void *dst, const void *a, const void *b, void *ctx) {
    NAD_UNUSED(ctx);

    *(int32_t *) dst = *(const int32_t *) a + *(const int32_t *) b;
}
/// [ops]

int main() {
    /// [map]
    const nad_Span src = NAD_SPAN_OF(int32_t, 1, 2, 3, 4);

    int64_t wide[4];
    const nad_SpanMut dst = NAD_SPAN_NEW_MUT(int64_t, wide, 4);

    // only the lengths have to match; the elem sizes are 4 and 8
    nad_span_transform(dst, src, widen_and_square, nullptr);
    nad_span_mut_print(dst, nad_fprint_i64); // [1, 4, 9, 16]

    // two sources in step
    const nad_Span other = NAD_SPAN_OF(int32_t, 10, 20, 30, 40);
    const nad_SpanMut sums = NAD_SPAN_OF_MUT(int32_t, 0, 0, 0, 0);

    nad_span_zip(sums, src, other, add, nullptr);
    nad_span_mut_print(sums, nad_fprint_i32); // [11, 22, 33, 44]
    /// [map]

    return 0;
}
