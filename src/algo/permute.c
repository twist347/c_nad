#include "nad/algo/permute.h"

#include "nad/algo/copy.h"

#include "internal/ptr.h"

#include <assert.h>

/* ========== internals ========== */

[[nodiscard]]
static bool permute_step(nad_SpanMut s, nad_Cmp cmp, bool asc);

/* ========== permute ========== */

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

    const nad_SpanMut right = nad_span_sub_mut(s, mid, s.len - mid);
    nad_span_reverse(right);

    nad_span_reverse(s);
}

void nad_span_swap_ranges(nad_SpanMut a, nad_SpanMut b) {
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(a.elem_size == b.elem_size);
    assert(a.len == b.len);

    if (a.len == 0 || a.data == b.data) {
        return;
    }

    for (size_t i = 0; i < a.len; ++i) {
        nad_bytes_swap(nad_span_get_mut(a, i), nad_span_get_mut(b, i), a.elem_size);
    }
}

bool nad_span_next_permutation(nad_SpanMut s, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);

    return permute_step(s, cmp, true);
}

bool nad_span_prev_permutation(nad_SpanMut s, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);

    return permute_step(s, cmp, false);
}

size_t nad_span_partition(nad_SpanMut s, nad_Pred pred, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(pred);

    const nad_Span view = nad_span_mut_to_span(s);
    size_t boundary = 0;

    for (size_t i = 0; i < s.len; ++i) {
        if (pred(nad_span_get(view, i), ctx)) {
            if (i != boundary) {
                nad_span_swap_elems(s, i, boundary);
            }
            ++boundary;
        }
    }
    return boundary;
}

nad_Status nad_span_partition_stable(
    nad_SpanMut s,
    nad_Pred pred,
    void *ctx,
    nad_Al *al,
    size_t *out_boundary
) {
    NAD_SPAN_ASSERT(s);
    assert(pred);
    assert(al);
    assert(out_boundary);

    // nothing to move and nothing to ask, so nothing to allocate either: an
    // allocator with no room left must still be able to partition an empty span
    if (s.len == 0) {
        *out_boundary = 0;
        return NAD_STATUS_OK;
    }

    const size_t bytes = s.len * s.elem_size;

    // room for the whole span, though only the rejected elems are ever put there:
    // how many those are is not known before pred has seen them all, and asking it
    // twice to find out would be a second, differently timed set of answers
    void *buf = nad_alloc(al, bytes);
    if (!buf) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    const nad_Span view = nad_span_mut_to_span(s);
    const nad_SpanMut rejected = nad_span_new_mut(buf, s.len, s.elem_size);

    size_t kept = 0, dropped = 0;

    for (size_t i = 0; i < s.len; ++i) {
        const void *elem = nad_span_get(view, i);

        if (pred(elem, ctx)) {
            // kept never runs ahead of i, so this only ever overwrites an elem
            // that has already been read
            if (kept != i) {
                nad_span_set(s, kept, elem);
            }
            ++kept;
        } else {
            nad_span_set(rejected, dropped, elem);
            ++dropped;
        }
    }

    // the front holds the kept ones in order, the rest of the span is free for the
    // rejected ones — also in order, since they were appended as they were met
    nad_span_copy(
        nad_span_sub_mut(s, kept, dropped),
        nad_span_sub(nad_span_mut_to_span(rejected), 0, dropped)
    );

    nad_dealloc(al, buf, bytes);

    *out_boundary = kept;
    return NAD_STATUS_OK;
}

bool nad_span_is_partitioned(nad_Span s, nad_Pred pred, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(pred);

    size_t i = 0;
    while (i < s.len && pred(nad_span_get(s, i), ctx)) {
        ++i;
    }
    while (i < s.len && !pred(nad_span_get(s, i), ctx)) {
        ++i;
    }

    return i == s.len;
}

/* ========== internals ========== */

static bool permute_step(nad_SpanMut s, nad_Cmp cmp, bool asc) {
    if (s.len < 2) {
        return false;
    }

    const nad_Span v = nad_span_mut_to_span(s);

    size_t pivot = s.len - 1;
    while (pivot > 0) {
        const int c = cmp(nad_span_get(v, pivot - 1), nad_span_get(v, pivot));
        if (asc ? c < 0 : c > 0) {
            break;
        }
        --pivot;
    }

    if (pivot == 0) {
        nad_span_reverse(s);
        return false;
    }
    --pivot;

    size_t mate = s.len - 1;
    while (true) {
        const int c = cmp(nad_span_get(v, mate), nad_span_get(v, pivot));
        if (asc ? c > 0 : c < 0) {
            break;
        }
        --mate;
    }

    nad_span_swap_elems(s, pivot, mate);

    nad_span_reverse(nad_span_sub_mut(s, pivot + 1, s.len - pivot - 1));
    return true;
}
