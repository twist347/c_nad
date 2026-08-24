// for @snippet

#include "nad/algo/fold.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/util.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/// [ops]
// the accumulator is the caller's and keeps the caller's type, which is how a span of
// int32_t folds into an int64_t
static void add_into(void *acc, const void *elem, void *ctx) {
    NAD_UNUSED(ctx);

    *(int64_t *) acc += *(const int32_t *) elem;
}

static void add(void *dst, const void *a, const void *b, void *ctx) {
    NAD_UNUSED(ctx);

    *(int32_t *) dst = *(const int32_t *) a + *(const int32_t *) b;
}

static void sub(void *dst, const void *a, const void *b, void *ctx) {
    NAD_UNUSED(ctx);

    *(int32_t *) dst = *(const int32_t *) a - *(const int32_t *) b;
}
/// [ops]

int main() {
    /// [fold]
    const nad_Span s = NAD_SPAN_OF(int32_t, 1, 2, 3, 4);

    // the initial value is the caller's, and an empty span would leave it alone — which
    // is what makes it the identity of the operation
    int64_t sum = 0;
    nad_span_fold(s, &sum, add_into, nullptr);
    printf("%" PRId64 "\n", sum); // 10

    // a scan keeps what a fold throws away
    int32_t buf[4];
    const nad_SpanMut running = NAD_SPAN_NEW_MUT(int32_t, buf, 4);

    nad_span_partial_sum(running, s, add, nullptr);
    nad_span_mut_print(running, nad_fprint_i32); // [1, 3, 6, 10]

    // and undoes it, given the inverse op
    int32_t back[4];
    const nad_SpanMut steps = NAD_SPAN_NEW_MUT(int32_t, back, 4);

    nad_span_adjacent_difference(steps, nad_span_mut_to_span(running), sub, nullptr);
    nad_span_mut_print(steps, nad_fprint_i32); // [1, 2, 3, 4] — back to where it started
    /// [fold]

    return 0;
}
