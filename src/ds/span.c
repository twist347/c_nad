#include "nad/ds/span.h"

#include <assert.h>
#include <string.h>

static const char *byte_offset(const void *base, size_t stride, size_t n);
static char *byte_offset_mut(void *base, size_t stride, size_t n);

/* ========== construction ========== */

nad_Span nad_span_new(const void *data, size_t len, size_t elem_size) {
    assert(data || len == 0);
    assert(elem_size > 0);

    return (nad_Span){
        .data = data,
        .len = len,
        .elem_size = elem_size
    };
}

nad_SpanMut nad_span_new_mut(void *data, size_t len, size_t elem_size) {
    assert(data || len == 0);
    assert(elem_size > 0);

    return (nad_SpanMut){
        .data = data,
        .len = len,
        .elem_size = elem_size
    };
}

nad_Span nad_span_from_mut(nad_SpanMut s) {
    NAD_SPAN_ASSERT(s);

    return (nad_Span){
        .data = s.data,
        .len = s.len,
        .elem_size = s.elem_size
    };
}

/* ========== subspan ========== */

nad_Span nad_span_sub(nad_Span s, size_t start, size_t count) {
    NAD_SPAN_ASSERT(s);
    assert(start <= s.len);
    assert(count <= s.len - start);

    return (nad_Span){
        .data = s.data ? byte_offset(s.data, s.elem_size, start) : nullptr,
        .len = count,
        .elem_size = s.elem_size
    };
}

nad_SpanMut nad_span_sub_mut(nad_SpanMut s, size_t start, size_t count) {
    NAD_SPAN_ASSERT(s);
    assert(start <= s.len);
    assert(count <= s.len - start);

    return (nad_SpanMut){
        .data = s.data ? byte_offset_mut(s.data, s.elem_size, start) : nullptr,
        .len = count,
        .elem_size = s.elem_size
    };
}

/* ========== info ========== */

size_t nad_span_bytes(nad_Span s) {
    NAD_SPAN_ASSERT(s);

    return s.len * s.elem_size;
}

/* ========== access ========== */

const void *nad_span_get(nad_Span s, size_t idx) {
    NAD_SPAN_ASSERT(s);
    assert(idx < s.len);

    return byte_offset(s.data, s.elem_size, idx);
}

void *nad_span_get_mut(nad_SpanMut s, size_t idx) {
    NAD_SPAN_ASSERT(s);
    assert(idx < s.len);

    return byte_offset_mut(s.data, s.elem_size, idx);
}

void nad_span_set(nad_SpanMut s, size_t idx, const void *val) {
    NAD_SPAN_ASSERT(s);
    assert(idx < s.len);
    assert(val);

    memcpy(byte_offset_mut(s.data, s.elem_size, idx), val, s.elem_size);
}

/* ========== mods ========== */

void nad_span_swap_elems(nad_SpanMut s, size_t i, size_t j) {
    NAD_SPAN_ASSERT(s);
    assert(i < s.len);
    assert(j < s.len);

    if (i == j) {
        return;
    }

    char *a = byte_offset_mut(s.data, s.elem_size, i);
    char *b = byte_offset_mut(s.data, s.elem_size, j);
    for (size_t k = 0; k < s.elem_size; ++k) {
        const char tmp = a[k];
        a[k] = b[k];
        b[k] = tmp;
    }
}

static const char *byte_offset(const void *base, size_t stride, size_t n) {
    return (const char *) base + stride * n;
}

static char *byte_offset_mut(void *base, size_t stride, size_t n) {
    return (char *) base + stride * n;
}
