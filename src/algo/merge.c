#include "terse/algo/merge.h"

#include "terse/algo/permute.h"
#include "terse/algo/search.h"

#include "internal/emit.h"

#include <assert.h>
#include <string.h>

/* ========== internals ========== */

static void merge_in_place(trs_SpanMut s, size_t mid, trs_Cmp cmp);

/// merges over 's' with the shorter run parked in 'buf', in one linear pass
static void merge_buffered(trs_SpanMut s, size_t mid, trs_Cmp cmp, void *buf);

/* ========== merge ========== */

void trs_span_merge(trs_SpanMut dst, trs_Span a, trs_Span b, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len == a.len + b.len);

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = trs_span_get(a, i);
        const void *r = trs_span_get(b, j);

        // '<=' takes from 'a' on a tie, which keeps equal elems in the order they
        // arrived and is what makes trs_span_sort_stable stable through this
        if (cmp(l, r) <= 0) {
            out = trs_emit(dst, out, l);
            ++i;
        } else {
            out = trs_emit(dst, out, r);
            ++j;
        }
    }

    // only one of the two has anything left, but which one is not known here
    out = trs_emit_rest(dst, out, a, i);
    out = trs_emit_rest(dst, out, b, j);

    assert(out == dst.len);
}

/* ========== inplace merge ========== */

void trs_span_inplace_merge(trs_SpanMut s, size_t mid, trs_Cmp cmp, trs_Al *al) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(mid <= s.len);

    const size_t left_len = mid;
    const size_t right_len = s.len - mid;

    if (left_len == 0 || right_len == 0) {
        return;
    }

    // the buffer holds the shorter run, so it is never more than half the span
    const size_t buf_len = left_len < right_len ? left_len : right_len;
    void *buf = al ? trs_alloc(al, buf_len * s.elem_size) : nullptr;

    if (!buf) {
        merge_in_place(s, mid, cmp);
        return;
    }

    merge_buffered(s, mid, cmp, buf);
    trs_dealloc(al, buf, buf_len * s.elem_size);
}

/* ========== internals ========== */

/*
 * The buffer-free merge, by the standard recursion. Take the middle elem of the longer
 * run and binary search for its place in the other one; that pair of cuts splits the
 * problem into two smaller merges whose results do not interleave. One rotate brings the
 * two inner pieces past each other, and the halves are merged the same way.
 *
 * Which run gets cut in the middle is what keeps it stable: cutting the left one searches
 * the right with lower_bound, so equal elems on the right land after, and cutting the
 * right one searches the left with upper_bound, so equal elems on the left stay before.
 *
 * Depth is logarithmic — each call halves the longer run — so the recursion needs no
 * unrolling into an explicit stack.
 */
static void merge_in_place(trs_SpanMut s, size_t mid, trs_Cmp cmp) {
    const size_t left_len = mid;
    const size_t right_len = s.len - mid;

    if (left_len == 0 || right_len == 0) {
        return;
    }

    if (s.len == 2) {
        const trs_Span cs = trs_span_mut_to_span(s);
        if (cmp(trs_span_get(cs, 1), trs_span_get(cs, 0)) < 0) {
            trs_span_swap_elems(s, 0, 1);
        }
        return;
    }

    const trs_Span cs = trs_span_mut_to_span(s);
    size_t left_cut;
    size_t right_cut;

    if (left_len > right_len) {
        left_cut = left_len / 2;
        right_cut =
                mid + trs_span_lower_bound(
                    trs_span_sub(cs, mid, right_len), trs_span_get(cs, left_cut), cmp
                );
    } else {
        right_cut = mid + right_len / 2;
        left_cut = trs_span_upper_bound(
            trs_span_sub(cs, 0, mid), trs_span_get(cs, right_cut), cmp
        );
    }

    // the two inner pieces trade places, and the boundary between what is already
    // settled on the left and what is settled on the right lands here
    trs_span_rotate(trs_span_sub_mut(s, left_cut, right_cut - left_cut), mid - left_cut);
    const size_t new_mid = left_cut + (right_cut - mid);

    merge_in_place(trs_span_sub_mut(s, 0, new_mid), left_cut, cmp);
    merge_in_place(trs_span_sub_mut(s, new_mid, s.len - new_mid), right_cut - new_mid, cmp);
}


/*
 * The linear path. Whichever run is shorter goes into 'buf', and the merge then runs over
 * the span itself in the direction that keeps the write position from overtaking the read
 * one: forward when the LEFT run was parked, backward when it was the right one.
 *
 * Either way the loop stops as soon as the buffer runs dry, because what is left of the
 * other run is already sitting where it belongs — the tail of the span is not touched at
 * all. Inside the loop the write position and the read position are never equal, so no
 * elem is ever copied onto itself.
 */
static void merge_buffered(trs_SpanMut s, size_t mid, trs_Cmp cmp, void *buf) {
    const size_t tsz = s.elem_size;
    const size_t left_len = mid;
    const size_t right_len = s.len - mid;
    const trs_Span cs = trs_span_mut_to_span(s);

    if (left_len <= right_len) {
        memcpy(buf, trs_span_get(cs, 0), left_len * tsz);
        const trs_Span parked = trs_span_from_data(buf, left_len, tsz);

        size_t w = 0;
        size_t b = 0;
        size_t r = mid;

        while (b < left_len && r < s.len) {
            // '<' keeps the parked left run ahead of an equal elem on the right
            const bool take_right = cmp(trs_span_get(cs, r), trs_span_get(parked, b)) < 0;

            memcpy(trs_span_get_mut(s, w), take_right ? trs_span_get(cs, r) : trs_span_get(parked, b), tsz);
            take_right ? ++r : ++b;
            ++w;
        }

        while (b < left_len) {
            memcpy(trs_span_get_mut(s, w), trs_span_get(parked, b), tsz);
            ++b;
            ++w;
        }

        return;
    }

    memcpy(buf, trs_span_get(cs, mid), right_len * tsz);
    const trs_Span parked = trs_span_from_data(buf, right_len, tsz);

    size_t w = s.len;
    size_t l = mid;
    size_t b = right_len;

    while (l > 0 && b > 0) {
        // '>' takes the left elem only when it is strictly greater, so on a tie the
        // parked right elem is placed later and the left run keeps its lead
        const bool take_left = cmp(trs_span_get(cs, l - 1), trs_span_get(parked, b - 1)) > 0;

        --w;
        if (take_left) {
            --l;
            memcpy(trs_span_get_mut(s, w), trs_span_get(cs, l), tsz);
        } else {
            --b;
            memcpy(trs_span_get_mut(s, w), trs_span_get(parked, b), tsz);
        }
    }

    while (b > 0) {
        --b;
        --w;
        memcpy(trs_span_get_mut(s, w), trs_span_get(parked, b), tsz);
    }
}
