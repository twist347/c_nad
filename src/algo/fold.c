#include "nad/algo/fold.h"

#include <assert.h>
#include <string.h>

/* ========== fold ========== */

void nad_span_fold(nad_Span s, void *acc, nad_Fold fold, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(acc);
    assert(fold);

    for (size_t i = 0; i < s.len; ++i) {
        fold(acc, nad_span_get(s, i), ctx);
    }
}

void nad_span_rfold(nad_Span s, void *acc, nad_Fold fold, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(acc);
    assert(fold);

    for (size_t i = s.len; i > 0; --i) {
        fold(acc, nad_span_get(s, i - 1), ctx);
    }
}

/* ========== scan ========== */

void nad_span_partial_sum(nad_SpanMut dst, nad_Span src, nad_BinOp op, void *ctx) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len == src.len);
    assert(op);

    if (src.len == 0) {
        return;
    }

    memcpy(nad_span_get_mut(dst, 0), nad_span_get(src, 0), dst.elem_size);

    const nad_Span prev = nad_span_mut_to_span(dst);

    for (size_t i = 1; i < src.len; ++i) {
        op(nad_span_get_mut(dst, i), nad_span_get(prev, i - 1), nad_span_get(src, i), ctx);
    }
}

void nad_span_adjacent_difference(nad_SpanMut dst, nad_Span src, nad_BinOp op, void *ctx) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len == src.len);
    assert(op);

    if (src.len == 0) {
        return;
    }

    memcpy(nad_span_get_mut(dst, 0), nad_span_get(src, 0), dst.elem_size);

    for (size_t i = 1; i < src.len; ++i) {
        op(nad_span_get_mut(dst, i), nad_span_get(src, i), nad_span_get(src, i - 1), ctx);
    }
}
