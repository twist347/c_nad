#include "nad/algo/merge.h"

#include "nad/algo/permute.h"
#include "nad/algo/search.h"

#include "internal/emit.h"

#include <assert.h>
#include <string.h>

/* ========== internals ========== */

static void merge_in_place(nad_SpanMut s, size_t mid, nad_Cmp cmp);

/// merges over 's' with the shorter run parked in 'buf', in one linear pass
static void merge_buffered(nad_SpanMut s, size_t mid, nad_Cmp cmp, void *buf);

/* ========== merge ========== */

void nad_span_merge(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len == a.len + b.len);

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = nad_span_get(a, i);
        const void *r = nad_span_get(b, j);

        // '<=' takes from 'a' on a tie, which keeps equal elems in the order they
        // arrived and is what makes nad_span_sort_stable stable through this
        if (cmp(l, r) <= 0) {
            out = nad_emit(dst, out, l);
            ++i;
        } else {
            out = nad_emit(dst, out, r);
            ++j;
        }
    }

    // only one of the two has anything left, but which one is not known here
    out = nad_emit_rest(dst, out, a, i);
    out = nad_emit_rest(dst, out, b, j);

    assert(out == dst.len);
}

/* ========== inplace merge ========== */

void nad_span_inplace_merge(nad_SpanMut s, size_t mid, nad_Cmp cmp, nad_Al *al) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);
    assert(mid <= s.len);

    const size_t left_len = mid;
    const size_t right_len = s.len - mid;

    if (left_len == 0 || right_len == 0) {
        return;
    }

    // the buffer holds the shorter run, so it is never more than half the span
    const size_t buf_len = left_len < right_len ? left_len : right_len;
    void *buf = al ? nad_alloc(al, buf_len * s.elem_size) : nullptr;

    if (!buf) {
        merge_in_place(s, mid, cmp);
        return;
    }

    merge_buffered(s, mid, cmp, buf);
    nad_dealloc(al, buf, buf_len * s.elem_size);
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
static void merge_in_place(nad_SpanMut s, size_t mid, nad_Cmp cmp) {
    const size_t left_len = mid;
    const size_t right_len = s.len - mid;

    if (left_len == 0 || right_len == 0) {
        return;
    }

    if (s.len == 2) {
        const nad_Span cs = nad_span_mut_to_span(s);
        if (cmp(nad_span_get(cs, 1), nad_span_get(cs, 0)) < 0) {
            nad_span_swap_elems(s, 0, 1);
        }
        return;
    }

    const nad_Span cs = nad_span_mut_to_span(s);
    size_t left_cut;
    size_t right_cut;

    if (left_len > right_len) {
        left_cut = left_len / 2;
        right_cut =
                mid + nad_span_lower_bound(
                    nad_span_sub(cs, mid, right_len), nad_span_get(cs, left_cut), cmp
                );
    } else {
        right_cut = mid + right_len / 2;
        left_cut = nad_span_upper_bound(
            nad_span_sub(cs, 0, mid), nad_span_get(cs, right_cut), cmp
        );
    }

    // the two inner pieces trade places, and the boundary between what is already
    // settled on the left and what is settled on the right lands here
    nad_span_rotate(nad_span_sub_mut(s, left_cut, right_cut - left_cut), mid - left_cut);
    const size_t new_mid = left_cut + (right_cut - mid);

    merge_in_place(nad_span_sub_mut(s, 0, new_mid), left_cut, cmp);
    merge_in_place(nad_span_sub_mut(s, new_mid, s.len - new_mid), right_cut - new_mid, cmp);
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
static void merge_buffered(nad_SpanMut s, size_t mid, nad_Cmp cmp, void *buf) {
    const size_t tsz = s.elem_size;
    const size_t left_len = mid;
    const size_t right_len = s.len - mid;
    const nad_Span cs = nad_span_mut_to_span(s);

    if (left_len <= right_len) {
        memcpy(buf, nad_span_get(cs, 0), left_len * tsz);
        const nad_Span parked = nad_span_new(buf, left_len, tsz);

        size_t w = 0;
        size_t b = 0;
        size_t r = mid;

        while (b < left_len && r < s.len) {
            // '<' keeps the parked left run ahead of an equal elem on the right
            const bool take_right = cmp(nad_span_get(cs, r), nad_span_get(parked, b)) < 0;

            memcpy(nad_span_get_mut(s, w), take_right ? nad_span_get(cs, r) : nad_span_get(parked, b), tsz);
            take_right ? ++r : ++b;
            ++w;
        }

        while (b < left_len) {
            memcpy(nad_span_get_mut(s, w), nad_span_get(parked, b), tsz);
            ++b;
            ++w;
        }

        return;
    }

    memcpy(buf, nad_span_get(cs, mid), right_len * tsz);
    const nad_Span parked = nad_span_new(buf, right_len, tsz);

    size_t w = s.len;
    size_t l = mid;
    size_t b = right_len;

    while (l > 0 && b > 0) {
        // '>' takes the left elem only when it is strictly greater, so on a tie the
        // parked right elem is placed later and the left run keeps its lead
        const bool take_left = cmp(nad_span_get(cs, l - 1), nad_span_get(parked, b - 1)) > 0;

        --w;
        if (take_left) {
            --l;
            memcpy(nad_span_get_mut(s, w), nad_span_get(cs, l), tsz);
        } else {
            --b;
            memcpy(nad_span_get_mut(s, w), nad_span_get(parked, b), tsz);
        }
    }

    while (b > 0) {
        --b;
        --w;
        memcpy(nad_span_get_mut(s, w), nad_span_get(parked, b), tsz);
    }
}
