#include "trs/algo/fill.h"

#include <assert.h>
#include <string.h>

void trs_span_fill(trs_SpanMut s, const void *val) {
    TRS_SPAN_ASSERT(s);
    assert(val);

    for (size_t i = 0; i < s.len; ++i) {
        memcpy(trs_span_get_mut(s, i), val, s.elem_size);
    }
}

void trs_span_fill_zero(trs_SpanMut s) {
    TRS_SPAN_ASSERT(s);

    if (s.len == 0) {
        return;
    }

    memset(s.data, 0, s.len * s.elem_size);
}

void trs_span_generate(trs_SpanMut s, trs_Gen gen, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(gen);

    for (size_t i = 0; i < s.len; ++i) {
        gen(trs_span_get_mut(s, i), i, ctx);
    }
}
