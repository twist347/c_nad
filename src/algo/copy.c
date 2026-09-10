#include "trs/algo/copy.h"

#include <assert.h>
#include <string.h>

void trs_span_copy(trs_SpanMut dst, trs_Span src) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len == src.len);

    if (dst.len == 0 || dst.data == src.data) {
        return;
    }

    memcpy(dst.data, src.data, dst.len * dst.elem_size);
}

size_t trs_span_copy_if(trs_SpanMut dst, trs_Span src, trs_Pred pred, void *ctx) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len >= src.len);
    assert(pred);

    size_t write = 0;

    for (size_t read = 0; read < src.len; ++read) {
        const void *cur = trs_span_get(src, read);
        if (pred(cur, ctx)) {
            memcpy(trs_span_get_mut(dst, write), cur, dst.elem_size);
            ++write;
        }
    }

    return write;
}

void trs_span_copy_overlapping(trs_SpanMut dst, trs_Span src) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len == src.len);

    if (dst.len == 0 || dst.data == src.data) {
        return;
    }

    memmove(dst.data, src.data, dst.len * dst.elem_size);
}
