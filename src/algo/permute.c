#include "trs/algo/permute.h"

#include "trs/algo/copy.h"

#include "internal/ptr.h"

#include <assert.h>

/* ========== internals ========== */

[[nodiscard]]
static bool permute_step(trs_SpanMut s, trs_Cmp cmp, bool asc);

/* ========== permute ========== */

void trs_span_reverse(trs_SpanMut s) {
    TRS_SPAN_ASSERT(s);

    if (s.len < 2) {
        return;
    }

    size_t left = 0, right = s.len - 1;

    while (left < right) {
        trs_span_swap_elems(s, left, right);
        ++left;
        --right;
    }
}

void trs_span_rotate(trs_SpanMut s, size_t mid) {
    TRS_SPAN_ASSERT(s);
    assert(mid <= s.len);

    if (mid == 0 || mid == s.len) {
        return;
    }

    const trs_SpanMut left = trs_span_sub_mut(s, 0, mid);
    trs_span_reverse(left);

    const trs_SpanMut right = trs_span_sub_mut(s, mid, s.len - mid);
    trs_span_reverse(right);

    trs_span_reverse(s);
}

void trs_span_swap_ranges(trs_SpanMut a, trs_SpanMut b) {
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(a.elem_size == b.elem_size);
    assert(a.len == b.len);

    if (a.len == 0 || a.data == b.data) {
        return;
    }

    for (size_t i = 0; i < a.len; ++i) {
        trs_bytes_swap(trs_span_get_mut(a, i), trs_span_get_mut(b, i), a.elem_size);
    }
}

bool trs_span_next_permutation(trs_SpanMut s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    return permute_step(s, cmp, true);
}

bool trs_span_prev_permutation(trs_SpanMut s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    return permute_step(s, cmp, false);
}

size_t trs_span_partition(trs_SpanMut s, trs_Pred pred, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(pred);

    const trs_Span view = trs_span_mut_to_span(s);
    size_t boundary = 0;

    for (size_t i = 0; i < s.len; ++i) {
        if (pred(trs_span_get(view, i), ctx)) {
            if (i != boundary) {
                trs_span_swap_elems(s, i, boundary);
            }
            ++boundary;
        }
    }
    return boundary;
}

trs_Status trs_span_partition_stable(
    trs_SpanMut s,
    trs_Pred pred,
    void *ctx,
    trs_Al *al,
    size_t *out_boundary
) {
    TRS_SPAN_ASSERT(s);
    assert(pred);
    assert(al);
    assert(out_boundary);

    // nothing to move and nothing to ask, so nothing to allocate either: an
    // allocator with no room left must still be able to partition an empty span
    if (s.len == 0) {
        *out_boundary = 0;
        return TRS_STATUS_OK;
    }

    const size_t bytes = s.len * s.elem_size;

    // room for the whole span, though only the rejected elems are ever put there:
    // how many those are is not known before pred has seen them all, and asking it
    // twice to find out would be a second, differently timed set of answers
    void *buf = trs_alloc(al, bytes);
    if (!buf) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    const trs_Span view = trs_span_mut_to_span(s);
    const trs_SpanMut rejected = trs_span_from_data_mut(buf, s.len, s.elem_size);

    size_t kept = 0, dropped = 0;

    for (size_t i = 0; i < s.len; ++i) {
        const void *elem = trs_span_get(view, i);

        if (pred(elem, ctx)) {
            // kept never runs ahead of i, so this only ever overwrites an elem
            // that has already been read
            if (kept != i) {
                trs_span_set(s, kept, elem);
            }
            ++kept;
        } else {
            trs_span_set(rejected, dropped, elem);
            ++dropped;
        }
    }

    // the front holds the kept ones in order, the rest of the span is free for the
    // rejected ones — also in order, since they were appended as they were met
    trs_span_copy(
        trs_span_sub_mut(s, kept, dropped),
        trs_span_sub(trs_span_mut_to_span(rejected), 0, dropped)
    );

    trs_dealloc(al, buf, bytes);

    *out_boundary = kept;
    return TRS_STATUS_OK;
}

bool trs_span_is_partitioned(trs_Span s, trs_Pred pred, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(pred);

    size_t i = 0;
    while (i < s.len && pred(trs_span_get(s, i), ctx)) {
        ++i;
    }
    while (i < s.len && !pred(trs_span_get(s, i), ctx)) {
        ++i;
    }

    return i == s.len;
}

void trs_span_shuffle(trs_SpanMut s, trs_Rng *rng) {
    TRS_SPAN_ASSERT(s);
    assert(rng);

    // walking down: each step settles position i - 1 by drawing from [0, i), the elems
    // not placed yet. Drawing from the whole span every step instead is the classic bug
    // — it looks the same and skews the result.
    for (size_t i = s.len; i > 1; --i) {
        trs_span_swap_elems(s, i - 1, trs_rng_idx(rng, i));
    }
}

void trs_span_shuffle_prefix(trs_SpanMut s, size_t count, trs_Rng *rng) {
    TRS_SPAN_ASSERT(s);
    assert(count <= s.len);
    assert(rng);

    // the same walk from the other end, so stopping early leaves the settled positions
    // at the front rather than the back
    for (size_t i = 0; i < count; ++i) {
        trs_span_swap_elems(s, i, i + trs_rng_idx(rng, s.len - i));
    }
}

/* ========== internals ========== */

static bool permute_step(trs_SpanMut s, trs_Cmp cmp, bool asc) {
    if (s.len < 2) {
        return false;
    }

    const trs_Span v = trs_span_mut_to_span(s);

    size_t pivot = s.len - 1;
    while (pivot > 0) {
        const int c = cmp(trs_span_get(v, pivot - 1), trs_span_get(v, pivot));
        if (asc ? c < 0 : c > 0) {
            break;
        }
        --pivot;
    }

    if (pivot == 0) {
        trs_span_reverse(s);
        return false;
    }
    --pivot;

    size_t mate = s.len - 1;
    while (true) {
        const int c = cmp(trs_span_get(v, mate), trs_span_get(v, pivot));
        if (asc ? c > 0 : c < 0) {
            break;
        }
        --mate;
    }

    trs_span_swap_elems(s, pivot, mate);

    trs_span_reverse(trs_span_sub_mut(s, pivot + 1, s.len - pivot - 1));
    return true;
}
