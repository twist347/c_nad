#include "trs/core/span.h"

#include "internal/ptr.h"

#include <assert.h>
#include <string.h>

/* ========== construction ========== */

trs_Span trs_span_from_data(const void *data, size_t len, size_t elem_size) {
    assert(data || len == 0);
    assert(elem_size > 0);

    return (trs_Span){
        .data = data,
        .len = len,
        .elem_size = elem_size
    };
}

trs_SpanMut trs_span_from_data_mut(void *data, size_t len, size_t elem_size) {
    assert(data || len == 0);
    assert(elem_size > 0);

    return (trs_SpanMut){
        .data = data,
        .len = len,
        .elem_size = elem_size
    };
}

/* ========== to span ========== */

trs_Span trs_span_mut_to_span(trs_SpanMut s) {
    TRS_SPAN_ASSERT(s);

    return (trs_Span){
        .data = s.data,
        .len = s.len,
        .elem_size = s.elem_size
    };
}

/* ========== subspan ========== */

trs_Span trs_span_sub(trs_Span self, size_t idx, size_t count) {
    TRS_SPAN_ASSERT(self);
    assert(idx <= self.len);
    assert(count <= self.len - idx);

    return (trs_Span){
        .data = self.data ? trs_byte_offset(self.data, self.elem_size, idx) : nullptr,
        .len = count,
        .elem_size = self.elem_size
    };
}

trs_SpanMut trs_span_sub_mut(trs_SpanMut self, size_t idx, size_t count) {
    TRS_SPAN_ASSERT(self);
    assert(idx <= self.len);
    assert(count <= self.len - idx);

    return (trs_SpanMut){
        .data = self.data ? trs_byte_offset_mut(self.data, self.elem_size, idx) : nullptr,
        .len = count,
        .elem_size = self.elem_size
    };
}

/* ========== info ========== */

size_t trs_span_bytes(trs_Span self) {
    TRS_SPAN_ASSERT(self);

    return self.len * self.elem_size;
}

/* ========== access ========== */

const void *trs_span_get(trs_Span self, size_t idx) {
    TRS_SPAN_ASSERT(self);
    assert(idx < self.len);

    return trs_byte_offset(self.data, self.elem_size, idx);
}

void *trs_span_get_mut(trs_SpanMut self, size_t idx) {
    TRS_SPAN_ASSERT(self);
    assert(idx < self.len);

    return trs_byte_offset_mut(self.data, self.elem_size, idx);
}

void trs_span_set(trs_SpanMut self, size_t idx, const void *val) {
    TRS_SPAN_ASSERT(self);
    assert(idx < self.len);
    assert(val);

    memcpy(trs_byte_offset_mut(self.data, self.elem_size, idx), val, self.elem_size);
}

/* ========== mods ========== */

void trs_span_swap_elems(trs_SpanMut self, size_t i, size_t j) {
    TRS_SPAN_ASSERT(self);
    assert(i < self.len);
    assert(j < self.len);

    if (i == j) {
        return;
    }

    trs_bytes_swap(
        trs_byte_offset_mut(self.data, self.elem_size, i),
        trs_byte_offset_mut(self.data, self.elem_size, j),
        self.elem_size
    );
}

/* ========== print ========== */

void trs_span_fprint(trs_Span self, FILE *stream, trs_FPrint fprint) {
    TRS_SPAN_ASSERT(self);
    assert(stream);
    assert(fprint);

    fputc('[', stream);
    for (size_t i = 0; i < self.len; ++i) {
        if (i > 0) {
            fputs(", ", stream);
        }
        fprint(stream, trs_span_get(self, i));
    }
    fputs("]\n", stream);
}

void trs_span_mut_fprint(trs_SpanMut self, FILE *stream, trs_FPrint fprint) {
    TRS_SPAN_ASSERT(self);
    assert(stream);
    assert(fprint);

    trs_span_fprint(trs_span_mut_to_span(self), stream, fprint);
}

void trs_span_print(trs_Span self, trs_FPrint fprint) {
    TRS_SPAN_ASSERT(self);
    assert(fprint);

    trs_span_fprint(self, stdout, fprint);
}

void trs_span_mut_print(trs_SpanMut self, trs_FPrint fprint) {
    TRS_SPAN_ASSERT(self);
    assert(fprint);

    trs_span_fprint(trs_span_mut_to_span(self), stdout, fprint);
}
