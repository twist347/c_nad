#include "trs/algo/compare.h"

#include <assert.h>
#include <string.h>

/* ========== equality ========== */

bool trs_span_eq(trs_Span a, trs_Span b) {
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(a.elem_size == b.elem_size);

    if (a.len != b.len) {
        return false;
    }

    if (a.len == 0 || a.data == b.data) {
        return true;
    }

    return memcmp(a.data, b.data, a.len * a.elem_size) == 0;
}

bool trs_span_eq_by(trs_Span a, trs_Span b, trs_Eq eq) {
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(a.elem_size == b.elem_size);
    assert(eq);

    if (a.len != b.len) {
        return false;
    }

    if (a.len == 0 || a.data == b.data) {
        return true;
    }

    for (size_t i = 0; i < a.len; ++i) {
        const void *x = trs_span_get(a, i);
        const void *y = trs_span_get(b, i);
        if (!eq(x, y)) {
            return false;
        }
    }
    return true;
}

bool trs_span_mismatch(trs_Span a, trs_Span b, trs_Eq eq, size_t *out_idx) {
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(eq);
    assert(out_idx);
    assert(a.elem_size == b.elem_size);

    if (a.data == b.data && a.len == b.len) {
        return false;
    }

    const size_t common = a.len < b.len ? a.len : b.len;

    for (size_t i = 0; i < common; ++i) {
        if (!eq(trs_span_get(a, i), trs_span_get(b, i))) {
            *out_idx = i;
            return true;
        }
    }
    return false;
}

/* ========== ordering ========== */

int trs_span_cmp(trs_Span a, trs_Span b, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(cmp);
    assert(a.elem_size == b.elem_size);

    if (a.data == b.data && a.len == b.len) {
        return 0;
    }

    const size_t common = a.len < b.len ? a.len : b.len;

    for (size_t i = 0; i < common; ++i) {
        const int c = cmp(trs_span_get(a, i), trs_span_get(b, i));
        if (c != 0) {
            return c < 0 ? -1 : 1;
        }
    }
    return (a.len > b.len) - (a.len < b.len);
}
