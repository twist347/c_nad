#include "nad/algo/transform.h"

#include <assert.h>

void nad_span_transform(nad_SpanMut dst, nad_Span src, nad_UnOp op, void *ctx) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(src);
    assert(dst.len == src.len);
    assert(op);

    for (size_t i = 0; i < src.len; ++i) {
        op(nad_span_get_mut(dst, i), nad_span_get(src, i), ctx);
    }
}

void nad_span_zip(nad_SpanMut dst, nad_Span a, nad_Span b, nad_BinOp op, void *ctx) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(dst.len == a.len);
    assert(a.len == b.len);
    assert(op);

    for (size_t i = 0; i < a.len; ++i) {
        op(nad_span_get_mut(dst, i), nad_span_get(a, i), nad_span_get(b, i), ctx);
    }
}
