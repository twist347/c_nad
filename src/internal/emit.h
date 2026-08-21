#pragma once

#include "nad/core/span.h"

#include <string.h>

/*
 * Appending into a span that is filled left to right, for the algorithms that walk two
 * sorted spans at once — algo/merge and algo/set. Both take the current write position
 * and report the next one, so the caller carries one cursor instead of two.
 */

/// appends one elem to 'dst' at 'out' and reports the next write position
[[nodiscard]]
static inline size_t nad_emit(nad_SpanMut dst, size_t out, const void *elem) {
    memcpy(nad_span_get_mut(dst, out), elem, dst.elem_size);

    return out + 1;
}

/// appends src[from..] and reports the next write position
[[nodiscard]]
static inline size_t nad_emit_rest(nad_SpanMut dst, size_t out, nad_Span src, size_t from) {
    if (from >= src.len) {
        return out;
    }

    const size_t n = src.len - from;
    memcpy(nad_span_get_mut(dst, out), nad_span_get(src, from), n * src.elem_size);

    return out + n;
}
