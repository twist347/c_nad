#include "nad/algo/compare.h"

#include <assert.h>
#include <string.h>

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

bool nad_span_eq_by(nad_Span a, nad_Span b, nad_EqFn eq) {
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
