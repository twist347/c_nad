// for @snippet

#include "nad/algo/fill.h"
#include "nad/algo/permute.h"
#include "nad/core/print.h"
#include "nad/core/rng.h"
#include "nad/core/span.h"

#include <stdint.h>
#include <stdio.h>

/// [gen]
// a nad_Gen that draws its elem instead of computing it. The generator rides in 'ctx',
// which is why algo needs no random fill of its own — this is it.
static void gen_roll(void *dst, size_t idx, void *ctx) {
    (void) idx;

    *(int32_t *) dst = nad_rng_i32_range(ctx, 1, 6);
}
/// [gen]

int main() {
    /// [seed]
    // a generator is a value: no allocator, no status, nothing to drop. The seed is the
    // whole state, so this run can be replayed exactly by passing the same one.
    nad_Rng rng = nad_rng_from_seed(2026);

    printf("%d %d\n", nad_rng_i32_range(&rng, 1, 6), nad_rng_i32_range(&rng, 1, 6)); // 4 3
    /// [seed]

    /// [draw]
    // uniform over what each one names, with no modulo bias anywhere
    const size_t idx = nad_rng_idx(&rng, 10);      // an index into ten elems
    const double unit = nad_rng_f64(&rng);         // [0.0, 1.0)
    const double angle = nad_rng_f64_range(&rng, 0.0, 6.283185307179586);
    const bool coin = nad_rng_bool(&rng);

    printf("%zu %.3f %.3f %s\n", idx, unit, angle, coin ? "heads" : "tails"); // 7 0.864 0.190 heads
    /// [draw]

    /// [span]
    int32_t rolls[12];
    nad_SpanMut all = NAD_SPAN_NEW_MUT(int32_t, rolls, 12);

    // filling and shuffling are algo's, and each takes the generator the way sort takes
    // a comparator
    nad_span_generate(all, gen_roll, &rng);
    nad_span_shuffle(all, &rng);

    // dealing three: only the front is settled, and it is a uniform sample of all twelve
    nad_span_shuffle_prefix(all, 3, &rng);
    nad_span_fprint(nad_span_sub(nad_span_mut_to_span(all), 0, 3), stdout, nad_fprint_i32); // [2, 6, 6]

    // one random elem needs no call of its own: an index and a get
    const nad_Span rolled = nad_span_mut_to_span(all);
    printf("%d\n", *(const int32_t *) nad_span_get(rolled, nad_rng_idx(&rng, 12))); // 5
    /// [span]

    return 0;
}
