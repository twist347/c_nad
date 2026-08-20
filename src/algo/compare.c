#include "nad/algo/compare.h"

#include <assert.h>
#include <string.h>

/* ========== equality ========== */

bool nad_span_eq(nad_Span a, nad_Span b) {
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(a.elem_size == b.elem_size);

    if (a.len != b.len) {
        return false;
    }

    if (a.len == 0 || a.data == b.data) {
        return true;
    }

    return memcmp(a.data, b.data, a.len * a.elem_size) == 0;
}

bool nad_span_eq_by(nad_Span a, nad_Span b, nad_Eq eq) {
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(a.elem_size == b.elem_size);
    assert(eq);

    if (a.len != b.len) {
        return false;
    }

    if (a.len == 0 || a.data == b.data) {
        return true;
    }

    for (size_t i = 0; i < a.len; ++i) {
        const void *x = nad_span_get(a, i);
        const void *y = nad_span_get(b, i);
        if (!eq(x, y)) {
            return false;
        }
    }
    return true;
}

bool nad_span_mismatch(nad_Span a, nad_Span b, nad_Eq eq, size_t *out_idx) {
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(eq);
    assert(out_idx);
    assert(a.elem_size == b.elem_size);

    if (a.data == b.data && a.len == b.len) {
        return false;
    }

    const size_t common = a.len < b.len ? a.len : b.len;

    for (size_t i = 0; i < common; ++i) {
        if (!eq(nad_span_get(a, i), nad_span_get(b, i))) {
            *out_idx = i;
            return true;
        }
    }
    return false;
}

/* ========== ordering ========== */

int nad_span_cmp(nad_Span a, nad_Span b, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(cmp);
    assert(a.elem_size == b.elem_size);

    if (a.data == b.data && a.len == b.len) {
        return 0;
    }

    const size_t common = a.len < b.len ? a.len : b.len;

    for (size_t i = 0; i < common; ++i) {
        const int c = cmp(nad_span_get(a, i), nad_span_get(b, i));
        if (c != 0) {
            return c < 0 ? -1 : 1;
        }
    }
    return (a.len > b.len) - (a.len < b.len);
}
