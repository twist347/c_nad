#include "nad/algo/copy.h"

#include <assert.h>
#include <string.h>

void nad_span_copy(nad_SpanMut dst, nad_Span src) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len == src.len);

    if (dst.len == 0 || dst.data == src.data) {
        return;
    }

    memcpy(dst.data, src.data, dst.len * dst.elem_size);
}

size_t nad_span_copy_if(nad_SpanMut dst, nad_Span src, nad_Pred pred, void *ctx) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len >= src.len);
    assert(pred);

    size_t write = 0;

    for (size_t read = 0; read < src.len; ++read) {
        const void *cur = nad_span_get(src, read);
        if (pred(cur, ctx)) {
            memcpy(nad_span_get_mut(dst, write), cur, dst.elem_size);
            ++write;
        }
    }

    return write;
}

void nad_span_copy_within(nad_SpanMut dst, nad_Span src) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(src);
    assert(dst.elem_size == src.elem_size);
    assert(dst.len == src.len);

    if (dst.len == 0 || dst.data == src.data) {
        return;
    }

    memmove(dst.data, src.data, dst.len * dst.elem_size);
}
