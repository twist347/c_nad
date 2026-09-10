#include "trs/algo/fold.h"

#include <assert.h>
#include <string.h>

/* ========== fold ========== */

void trs_span_fold(trs_Span s, void *acc, trs_Fold fold, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(acc);
    assert(fold);

    for (size_t i = 0; i < s.len; ++i) {
        fold(acc, trs_span_get(s, i), ctx);
    }
}

void trs_span_fold_back(trs_Span s, void *acc, trs_Fold fold, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(acc);
    assert(fold);

    for (size_t i = s.len; i > 0; --i) {
        fold(acc, trs_span_get(s, i - 1), ctx);
    }
}

/* ========== scan ========== */

void trs_span_partial_sum(trs_SpanMut dst, trs_Span src, trs_BinOp op, void *ctx) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len == src.len);
    assert(op);

    if (src.len == 0) {
        return;
    }

    memcpy(trs_span_get_mut(dst, 0), trs_span_get(src, 0), dst.elem_size);

    const trs_Span prev = trs_span_mut_to_span(dst);

    for (size_t i = 1; i < src.len; ++i) {
        op(trs_span_get_mut(dst, i), trs_span_get(prev, i - 1), trs_span_get(src, i), ctx);
    }
}

void trs_span_adjacent_difference(trs_SpanMut dst, trs_Span src, trs_BinOp op, void *ctx) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len == src.len);
    assert(op);

    if (src.len == 0) {
        return;
    }

    memcpy(trs_span_get_mut(dst, 0), trs_span_get(src, 0), dst.elem_size);

    for (size_t i = 1; i < src.len; ++i) {
        op(trs_span_get_mut(dst, i), trs_span_get(src, i), trs_span_get(src, i - 1), ctx);
    }
}
