#include "tda/core/span.h"

#include "internal/ptr.h"

#include <assert.h>
#include <string.h>

/* ========== construction ========== */

tda_Span tda_span_from_data(const void *data, size_t len, size_t elem_size) {
    assert(data || len == 0);
    assert(elem_size > 0);

    return (tda_Span){
        .data = data,
        .len = len,
        .elem_size = elem_size
    };
}

tda_SpanMut tda_span_from_data_mut(void *data, size_t len, size_t elem_size) {
    assert(data || len == 0);
    assert(elem_size > 0);

    return (tda_SpanMut){
        .data = data,
        .len = len,
        .elem_size = elem_size
    };
}

/* ========== to span ========== */

tda_Span tda_span_mut_to_span(tda_SpanMut s) {
    TDA_SPAN_ASSERT(s);

    return (tda_Span){
        .data = s.data,
        .len = s.len,
        .elem_size = s.elem_size
    };
}

/* ========== subspan ========== */

tda_Span tda_span_sub(tda_Span self, size_t idx, size_t count) {
    TDA_SPAN_ASSERT(self);
    assert(idx <= self.len);
    assert(count <= self.len - idx);

    return (tda_Span){
        .data = self.data ? tda_byte_offset(self.data, self.elem_size, idx) : nullptr,
        .len = count,
        .elem_size = self.elem_size
    };
}

tda_SpanMut tda_span_sub_mut(tda_SpanMut self, size_t idx, size_t count) {
    TDA_SPAN_ASSERT(self);
    assert(idx <= self.len);
    assert(count <= self.len - idx);

    return (tda_SpanMut){
        .data = self.data ? tda_byte_offset_mut(self.data, self.elem_size, idx) : nullptr,
        .len = count,
        .elem_size = self.elem_size
    };
}

/* ========== info ========== */

size_t tda_span_bytes(tda_Span self) {
    TDA_SPAN_ASSERT(self);

    return self.len * self.elem_size;
}

/* ========== access ========== */

const void *tda_span_get(tda_Span self, size_t idx) {
    TDA_SPAN_ASSERT(self);
    assert(idx < self.len);

    return tda_byte_offset(self.data, self.elem_size, idx);
}

void *tda_span_get_mut(tda_SpanMut self, size_t idx) {
    TDA_SPAN_ASSERT(self);
    assert(idx < self.len);

    return tda_byte_offset_mut(self.data, self.elem_size, idx);
}

void tda_span_set(tda_SpanMut self, size_t idx, const void *val) {
    TDA_SPAN_ASSERT(self);
    assert(idx < self.len);
    assert(val);

    memcpy(tda_byte_offset_mut(self.data, self.elem_size, idx), val, self.elem_size);
}

/* ========== mods ========== */

void tda_span_swap_elems(tda_SpanMut self, size_t i, size_t j) {
    TDA_SPAN_ASSERT(self);
    assert(i < self.len);
    assert(j < self.len);

    if (i == j) {
        return;
    }

    tda_bytes_swap(
        tda_byte_offset_mut(self.data, self.elem_size, i),
        tda_byte_offset_mut(self.data, self.elem_size, j),
        self.elem_size
    );
}

/* ========== print ========== */

void tda_span_fprint(tda_Span self, FILE *stream, tda_FPrint fprint) {
    TDA_SPAN_ASSERT(self);
    assert(stream);
    assert(fprint);

    fputc('[', stream);
    for (size_t i = 0; i < self.len; ++i) {
        if (i > 0) {
            fputs(", ", stream);
        }
        fprint(stream, tda_span_get(self, i));
    }
    fputs("]\n", stream);
}

void tda_span_mut_fprint(tda_SpanMut self, FILE *stream, tda_FPrint fprint) {
    TDA_SPAN_ASSERT(self);
    assert(stream);
    assert(fprint);

    tda_span_fprint(tda_span_mut_to_span(self), stream, fprint);
}

void tda_span_print(tda_Span self, tda_FPrint fprint) {
    TDA_SPAN_ASSERT(self);
    assert(fprint);

    tda_span_fprint(self, stdout, fprint);
}

void tda_span_mut_print(tda_SpanMut self, tda_FPrint fprint) {
    TDA_SPAN_ASSERT(self);
    assert(fprint);

    tda_span_fprint(tda_span_mut_to_span(self), stdout, fprint);
}
