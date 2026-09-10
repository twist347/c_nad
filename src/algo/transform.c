#include "trs/algo/transform.h"

#include <assert.h>

void trs_span_transform(trs_SpanMut dst, trs_Span src, trs_UnOp op, void *ctx) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(src);
    assert(dst.len == src.len);
    assert(op);

    for (size_t i = 0; i < src.len; ++i) {
        op(trs_span_get_mut(dst, i), trs_span_get(src, i), ctx);
    }
}

void trs_span_zip_with(trs_SpanMut dst, trs_Span a, trs_Span b, trs_BinOp op, void *ctx) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(dst.len == a.len);
    assert(a.len == b.len);
    assert(op);

    for (size_t i = 0; i < a.len; ++i) {
        op(trs_span_get_mut(dst, i), trs_span_get(a, i), trs_span_get(b, i), ctx);
    }
}
