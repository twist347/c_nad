// for @snippet

#include "trs/algo/permute.h"
#include "trs/alloc/default.h"
#include "trs/core/print.h"
#include "trs/core/rng.h"
#include "trs/core/span.h"
#include "trs/core/util.h"

#include <stdint.h>
#include <stdio.h>

/// [pred]
static bool is_even(const void *elem, void *ctx) {
    TRS_UNUSED(ctx);

    return *(const int32_t *) elem % 2 == 0;
}

/// [pred]

int main() {
    /// [move]
    const trs_SpanMut s = TRS_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5);

    trs_span_reverse(s);
    trs_span_mut_print(s, trs_fprint_i32); // [5, 4, 3, 2, 1]

    // rotate left so that the elem at 'mid' becomes the first
    trs_span_rotate(s, 2);
    trs_span_mut_print(s, trs_fprint_i32); // [3, 2, 1, 5, 4]
    /// [move]

    /// [shuffle]
    // the order comes from a generator, so the same seed replays the same shuffle
    trs_Rng rng = trs_rng_from_seed(2026);
    const trs_SpanMut deck = TRS_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5, 6, 7, 8);

    trs_span_shuffle(deck, &rng);
    trs_span_mut_print(deck, trs_fprint_i32); // [8, 2, 3, 1, 6, 7, 5, 4]

    // dealing a hand: only the first three positions are settled, and they are a uniform
    // sample of the whole span rather than of its front
    trs_span_shuffle_prefix(deck, 3, &rng);
    trs_span_mut_print(trs_span_sub_mut(deck, 0, 3), trs_fprint_i32); // [4, 5, 6]
    /// [shuffle]

    /// [partition]
    const trs_SpanMut t = TRS_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5, 6);

    // the fast split: it swaps, so neither side keeps the order it had
    const size_t at = trs_span_partition(t, is_even, nullptr);
    printf("%zu even, ", at);
    trs_span_mut_print(t, trs_fprint_i32); // 3 even, [2, 4, 6, 1, 5, 3]

    // keeping both orders costs a buffer, so that one takes an allocator and can fail
    const trs_SpanMut u = TRS_SPAN_OF_MUT(int32_t, 1, 2, 3, 4, 5, 6);
    size_t boundary;
    if (TRS_STATUS_IS_ERR(trs_span_partition_stable(u, is_even, nullptr, trs_al_default(), &boundary))) {
        return 1;
    }
    printf("%zu even, ", boundary);
    trs_span_mut_print(u, trs_fprint_i32); // 3 even, [2, 4, 6, 1, 3, 5]
    /// [partition]

    return 0;
}
