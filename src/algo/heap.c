#include "trs/algo/heap.h"

#include <assert.h>

/* ========== internals ========== */

/// walks 'idx' towards the root while it outranks its parent
static void sift_up(trs_SpanMut s, size_t idx, trs_Cmp cmp);

/// walks 'idx' towards the leaves of the heap held in the first 'len' elems.
/// 'len' is a parameter rather than s.len because pop_heap and sort_heap shrink
/// the heap while leaving the popped elems in the span behind it
static void sift_down(trs_SpanMut s, size_t idx, size_t len, trs_Cmp cmp);

/* ========== build ========== */

void trs_span_make_heap(trs_SpanMut s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    // leaves are heaps already, so the work starts at the last elem that has a child
    for (size_t i = s.len / 2; i > 0; --i) {
        sift_down(s, i - 1, s.len, cmp);
    }
}

/* ========== push / pop ========== */

void trs_span_push_heap(trs_SpanMut s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(s.len > 0);

    sift_up(s, s.len - 1, cmp);
}

void trs_span_pop_heap(trs_SpanMut s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(s.len > 0);

    if (s.len == 1) {
        return;
    }

    trs_span_swap_elems(s, 0, s.len - 1);
    sift_down(s, 0, s.len - 1, cmp);
}

/* ========== sort ========== */

void trs_span_sort_heap(trs_SpanMut s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    // each pop parks one more largest elem behind a heap one shorter
    for (size_t len = s.len; len > 1; --len) {
        trs_span_pop_heap(trs_span_sub_mut(s, 0, len), cmp);
    }
}

/* ========== predicates ========== */

bool trs_span_is_heap(trs_Span s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    return trs_span_is_heap_until(s, cmp) == s.len;
}

size_t trs_span_is_heap_until(trs_Span s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    // every elem but the root is somebody's child, so checking each child against its
    // parent covers every edge of the tree exactly once
    for (size_t child = 1; child < s.len; ++child) {
        const size_t parent = (child - 1) / 2;

        if (cmp(trs_span_get(s, parent), trs_span_get(s, child)) < 0) {
            return child;
        }
    }

    return s.len;
}

/* ========== internals ========== */

static void sift_up(trs_SpanMut s, size_t idx, trs_Cmp cmp) {
    const trs_Span cs = trs_span_mut_to_span(s);

    while (idx > 0) {
        const size_t parent = (idx - 1) / 2;

        if (cmp(trs_span_get(cs, parent), trs_span_get(cs, idx)) >= 0) {
            break;
        }

        trs_span_swap_elems(s, parent, idx);
        idx = parent;
    }
}

static void sift_down(trs_SpanMut s, size_t idx, size_t len, trs_Cmp cmp) {
    assert(len <= s.len);

    const trs_Span cs = trs_span_mut_to_span(s);

    for (;;) {
        const size_t left = 2 * idx + 1;
        if (left >= len) {
            break;
        }

        // on a tie the left child wins, which is what keeps pop_heap from
        // reshuffling equal elems for no reason
        size_t best = left;
        const size_t right = left + 1;
        if (right < len && cmp(trs_span_get(cs, left), trs_span_get(cs, right)) < 0) {
            best = right;
        }

        if (cmp(trs_span_get(cs, idx), trs_span_get(cs, best)) >= 0) {
            break;
        }

        trs_span_swap_elems(s, idx, best);
        idx = best;
    }
}
