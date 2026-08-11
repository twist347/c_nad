#include "nad/algo/modify.h"

#include <assert.h>
#include <string.h>

void nad_span_reverse(nad_SpanMut s) {
    NAD_SPAN_ASSERT(s);

    if (s.len < 2) {
        return;
    }

    size_t left = 0, right = s.len - 1;

    while (left < right) {
        nad_span_swap_elems(s, left, right);
        ++left;
        --right;
    }
}

void nad_span_rotate(nad_SpanMut s, size_t mid) {
    NAD_SPAN_ASSERT(s);
    assert(mid <= s.len);

    if (mid == 0 || mid == s.len) {
        return;
    }

    const nad_SpanMut left = nad_span_sub_mut(s, 0, mid);
    nad_span_reverse(left);

    const nad_SpanMut right = nad_span_sub_mut(s, mid, s.len - 1);
    nad_span_reverse(right);

    nad_span_reverse(s);
}

