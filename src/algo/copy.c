#include "nad/algo/copy.h"

#include <assert.h>
#include <string.h>

/* ========== copy ========== */

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