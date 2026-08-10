#include "nad/algo/fill.h"

#include <assert.h>
#include <string.h>

/* ========== fill ========== */

void nad_span_fill(nad_SpanMut s, const void *val) {
    NAD_SPAN_ASSERT(s);
    assert(val);

    for (size_t i = 0; i < s.len; ++i) {
        memcpy(nad_span_get_mut(s, i), val, s.elem_size);
    }
}

void nad_span_fill_zero(nad_SpanMut s) {
    NAD_SPAN_ASSERT(s);

    if (s.len == 0) {
        return;
    }

    memset(s.data, 0, s.len * s.elem_size);
}
