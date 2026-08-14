#include "nad/algo/merge.h"

#include <assert.h>
#include <string.h>

void nad_span_merge(nad_SpanMut dst, nad_Span a, nad_Span b, nad_CmpFn cmp) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len == a.len + b.len);
    assert(cmp);

    const size_t tsz = dst.elem_size;
    size_t i = 0;
    size_t j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = nad_span_get(a, i);
        const void *r = nad_span_get(b, j);

        if (cmp(l, r) <= 0) {
            memcpy(nad_span_get_mut(dst, out), l, tsz);
            ++i;
        } else {
            memcpy(nad_span_get_mut(dst, out), r, tsz);
            ++j;
        }
        ++out;
    }

    while (i < a.len) {
        memcpy(nad_span_get_mut(dst, out), nad_span_get(a, i), tsz);
        ++i;
        ++out;
    }

    while (j < b.len) {
        memcpy(nad_span_get_mut(dst, out), nad_span_get(b, j), tsz);
        ++j;
        ++out;
    }
}
