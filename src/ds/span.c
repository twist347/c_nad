#include "nad/ds/span.h"

#include "internal/ptr.h"

#include <assert.h>
#include <string.h>

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

nad_Span nad_span_sub(nad_Span self, size_t start, size_t count) {
    NAD_SPAN_ASSERT(self);
    assert(start <= self.len);
    assert(count <= self.len - start);

    return (nad_Span){
        .data = self.data ? nad_byte_offset(self.data, self.elem_size, start) : nullptr,
        .len = count,
        .elem_size = self.elem_size
    };
}

nad_SpanMut nad_span_sub_mut(nad_SpanMut self, size_t start, size_t count) {
    NAD_SPAN_ASSERT(self);
    assert(start <= self.len);
    assert(count <= self.len - start);

    return (nad_SpanMut){
        .data = self.data ? nad_byte_offset_mut(self.data, self.elem_size, start) : nullptr,
        .len = count,
        .elem_size = self.elem_size
    };
}

/* ========== info ========== */

size_t nad_span_bytes(nad_Span self) {
    NAD_SPAN_ASSERT(self);

    return self.len * self.elem_size;
}

/* ========== access ========== */

const void *nad_span_get(nad_Span self, size_t idx) {
    NAD_SPAN_ASSERT(self);
    assert(idx < self.len);

    return nad_byte_offset(self.data, self.elem_size, idx);
}

void *nad_span_get_mut(nad_SpanMut self, size_t idx) {
    NAD_SPAN_ASSERT(self);
    assert(idx < self.len);

    return nad_byte_offset_mut(self.data, self.elem_size, idx);
}

void nad_span_set(nad_SpanMut self, size_t idx, const void *val) {
    NAD_SPAN_ASSERT(self);
    assert(idx < self.len);
    assert(val);

    memcpy(nad_byte_offset_mut(self.data, self.elem_size, idx), val, self.elem_size);
}

/* ========== mods ========== */

void nad_span_swap_elems(nad_SpanMut self, size_t i, size_t j) {
    NAD_SPAN_ASSERT(self);
    assert(i < self.len);
    assert(j < self.len);

    if (i == j) {
        return;
    }

    nad_bytes_swap(
        nad_byte_offset_mut(self.data, self.elem_size, i),
        nad_byte_offset_mut(self.data, self.elem_size, j),
        self.elem_size
    );
}

/* ========== print ========== */

void nad_span_fprint(nad_Span self, FILE *stream, nad_FPrint fprint) {
    NAD_SPAN_ASSERT(self);
    assert(stream);
    assert(fprint);

    fputc('[', stream);
    for (size_t i = 0; i < self.len; ++i) {
        if (i > 0) {
            fputs(", ", stream);
        }
        fprint(stream, nad_span_get(self, i));
    }
    fputs("]\n", stream);
}

void nad_span_mut_fprint(nad_SpanMut self, FILE *stream, nad_FPrint fprint) {
    nad_span_fprint(nad_span_from_mut(self), stream, fprint);
}

void nad_span_print(nad_Span self, nad_FPrint fprint) {
    nad_span_fprint(self, stdout, fprint);
}

void nad_span_mut_print(nad_SpanMut self, nad_FPrint fprint) {
    nad_span_fprint(nad_span_from_mut(self), stdout, fprint);
}
