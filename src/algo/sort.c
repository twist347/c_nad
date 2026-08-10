#include "nad/algo/sort.h"

#include <assert.h>

/* ========== sort ========== */

void nad_span_insertion_sort(nad_SpanMut s, nad_cmp_fn cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);

    const nad_Span cs = nad_span_from_mut(s);
    for (size_t i = 1; i < s.len; ++i) {
        for (size_t j = i; j > 0; --j) {
            const void *prev = nad_span_get(cs, j - 1);
            const void *cur = nad_span_get(cs, j);

            if (cmp(prev, cur) <= 0) {
                break;
            }
            nad_span_swap_elems(s, j - 1, j);
        }
    }
}

/* ========== info ========== */

bool nad_span_is_sorted(nad_Span s, nad_cmp_fn cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);

    for (size_t i = 1; i < s.len; ++i) {
        const void *prev = nad_span_get(s, i - 1);
        const void *cur = nad_span_get(s, i);
        if (cmp(prev, cur) > 0) {
            return false;
        }
    }
    return true;
}
