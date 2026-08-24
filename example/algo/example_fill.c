// for @snippet

#include "nad/algo/fill.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/util.h"

#include <stdint.h>

/// [gen]
// a generator is handed the index it is filling, which is enough for anything positional
static void squares(void *dst, size_t idx, void *ctx) {
    NAD_UNUSED(ctx);

    *(int32_t *) dst = (int32_t) (idx * idx);
}
/// [gen]

int main() {
    /// [fill]
    int32_t buf[5];
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_fill(s, &(int32_t){ 7 });
    nad_span_mut_print(s, nad_fprint_i32); // [7, 7, 7, 7, 7]

    nad_span_fill_zero(s);
    nad_span_mut_print(s, nad_fprint_i32); // [0, 0, 0, 0, 0]

    nad_span_generate(s, squares, nullptr);
    nad_span_mut_print(s, nad_fprint_i32); // [0, 1, 4, 9, 16]
    /// [fill]

    return 0;
}
