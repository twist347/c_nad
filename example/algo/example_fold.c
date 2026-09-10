// for @snippet

#include "trs/algo/fold.h"
#include "trs/core/print.h"
#include "trs/core/span.h"
#include "trs/core/util.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/// [ops]
static void add_into(void *acc, const void *elem, void *ctx) {
    TRS_UNUSED(ctx);

    *(int64_t *) acc += *(const int32_t *) elem;
}

static void add(void *dst, const void *a, const void *b, void *ctx) {
    TRS_UNUSED(ctx);

    *(int32_t *) dst = *(const int32_t *) a + *(const int32_t *) b;
}

static void sub(void *dst, const void *a, const void *b, void *ctx) {
    TRS_UNUSED(ctx);

    *(int32_t *) dst = *(const int32_t *) a - *(const int32_t *) b;
}

/// [ops]

int main() {
    /// [fold]
    const trs_Span s = TRS_SPAN_OF(int32_t, 1, 2, 3, 4);

    // the initial value is the caller's, and an empty span would leave it alone — which
    // is what makes it the identity of the operation
    int64_t sum = 0;
    trs_span_fold(s, &sum, add_into, nullptr);
    printf("%" PRId64 "\n", sum); // 10

    // a scan keeps what a fold throws away
    int32_t buf[4];
    const trs_SpanMut running = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 4);

    trs_span_partial_sum(running, s, add, nullptr);
    trs_span_mut_print(running, trs_fprint_i32); // [1, 3, 6, 10]

    // and undoes it, given the inverse op
    int32_t back[4];
    const trs_SpanMut steps = TRS_SPAN_FROM_DATA_MUT(int32_t, back, 4);

    trs_span_adjacent_difference(steps, trs_span_mut_to_span(running), sub, nullptr);
    trs_span_mut_print(steps, trs_fprint_i32); // [1, 2, 3, 4] — back to where it started
    /// [fold]

    return 0;
}
