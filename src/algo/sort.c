#include "trs/algo/sort.h"

#include "trs/algo/copy.h"
#include "trs/algo/merge.h"
#include "trs/core/util.h"

#include <assert.h>

/* ========== internals ========== */

// below this many elems insertion sort wins: no partitioning overhead, and the range
// is short enough that its quadratic cost does not show
static constexpr size_t INSERTION_THRESHOLD = 16;

[[nodiscard]]
static size_t median3(trs_Span s, size_t a, size_t b, size_t c, trs_Cmp cmp);

/// median of three medians, sampled across the whole range
[[nodiscard]]
static size_t ninther(trs_Span s, size_t left, size_t right, trs_Cmp cmp);

// the block of elems equal to the pivot after a three-way split, as the
// inclusive range [lt, gt]: everything below lt is smaller, above gt larger
typedef struct {
    size_t lt;
    size_t gt;
} Split;

/// splits [left, right] into < pivot | == pivot | > pivot and returns the
/// bounds of the middle run
[[nodiscard]]
static Split partition3(trs_SpanMut s, size_t left, size_t right, size_t pivot_idx, trs_Cmp cmp);

/// sorts the inclusive range [left, right]
static void quicksort(trs_SpanMut s, size_t left, size_t right, trs_Cmp cmp);

/* ========== sort ========== */

void trs_span_insertion_sort(trs_SpanMut s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    const trs_Span cs = trs_span_mut_to_span(s);
    for (size_t i = 1; i < s.len; ++i) {
        for (size_t j = i; j > 0; --j) {
            const void *prev = trs_span_get(cs, j - 1);
            const void *cur = trs_span_get(cs, j);
            if (cmp(prev, cur) <= 0) {
                break;
            }
            trs_span_swap_elems(s, j - 1, j);
        }
    }
}

void trs_span_sort(trs_SpanMut s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    if (s.len < 2) {
        return;
    }

    quicksort(s, 0, s.len - 1, cmp);
}

trs_Status trs_span_sort_stable(trs_SpanMut s, trs_Cmp cmp, trs_Al *al) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(al);

    if (s.len < 2) {
        return TRS_STATUS_OK;
    }

    const size_t bytes = s.len * s.elem_size;

    void *buf = trs_alloc(al, bytes);
    if (!buf) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    trs_SpanMut src = s;
    trs_SpanMut dst = trs_span_from_data_mut(buf, s.len, s.elem_size);

    // bottom-up merge sort: merge runs of width 1, 2, 4, ...
    for (size_t width = 1; width < s.len; width *= 2) {
        for (size_t i = 0; i < s.len; i += 2 * width) {
            const size_t mid = i + width < s.len ? i + width : s.len;
            const size_t end = i + 2 * width < s.len ? i + 2 * width : s.len;

            const trs_Span run = trs_span_mut_to_span(src);
            trs_span_merge(
                trs_span_sub_mut(dst, i, end - i),
                trs_span_sub(run, i, mid - i),
                trs_span_sub(run, mid, end - mid),
                cmp
            );
        }
        TRS_SWAP(src, dst);
    }

    // if result ended up in buf, copy back to original
    if (src.data != s.data) {
        trs_span_copy(s, trs_span_mut_to_span(src));
    }

    trs_dealloc(al, buf, bytes);

    return TRS_STATUS_OK;
}

void trs_span_partial_sort(trs_SpanMut s, size_t count, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(count <= s.len);

    if (count == 0 || s.len < 2) {
        return;
    }

    if (count >= s.len) {
        trs_span_sort(s, cmp);
        return;
    }

    // place element that would be at position count in sorted order
    trs_span_nth_elem(s, count, cmp);

    // now the first count elems are the count smallest (order unspecified) -> sort them
    trs_span_sort(trs_span_sub_mut(s, 0, count), cmp);
}

void trs_span_nth_elem(trs_SpanMut s, size_t nth, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(nth < s.len);

    if (s.len < 2) {
        return;
    }

    size_t left = 0, right = s.len - 1;

    // left <= nth <= right holds every round, which is what keeps lt - 1 and gt + 1
    // inside the range below
    while (left < right) {
        const size_t pivot_idx = ninther(trs_span_mut_to_span(s), left, right, cmp);

        const Split p = partition3(s, left, right, pivot_idx, cmp);
        const size_t lt = p.lt;
        const size_t gt = p.gt;

        if (nth < lt) {
            right = lt - 1;
        } else if (nth > gt) {
            left = gt + 1;
        } else {
            return; // nth landed inside the run of elems equal to the pivot
        }
    }
}

/* ========== info ========== */

bool trs_span_is_sorted(trs_Span s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    return trs_span_is_sorted_until(s, cmp) == s.len;
}

size_t trs_span_is_sorted_until(trs_Span s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);

    for (size_t i = 1; i < s.len; ++i) {
        const void *prev = trs_span_get(s, i - 1);
        const void *cur = trs_span_get(s, i);
        if (cmp(prev, cur) > 0) {
            return i;
        }
    }
    return s.len;
}

/* ========== internals ========== */

static size_t median3(trs_Span s, size_t a, size_t b, size_t c, trs_Cmp cmp) {
    const void *a_ptr = trs_span_get(s, a);
    const void *b_ptr = trs_span_get(s, b);
    const void *c_ptr = trs_span_get(s, c);

    const int ab = cmp(a_ptr, b_ptr);
    const int ac = cmp(a_ptr, c_ptr);
    const int bc = cmp(b_ptr, c_ptr);

    // A between B and C: either c <= a <= b, or b <= a <= c
    if ((ab <= 0 && ac >= 0) || (ab >= 0 && ac <= 0)) return a;
    // B between A and C: either a <= b <= c, or c <= b <= a
    if ((ab <= 0 && bc <= 0) || (ab >= 0 && bc >= 0)) return b;
    // else C is median
    return c;
}

static size_t ninther(trs_Span s, size_t left, size_t right, trs_Cmp cmp) {
    const size_t len = right - left + 1;
    const size_t mid = left + len / 2;

    // on a range shorter than 8 the step is 0, every triple collapses to a single elem
    // and this degenerates into a plain median of left, mid and right — still valid,
    // just less well sampled. nth_elem reaches those short ranges; quicksort does not.
    const size_t step = len / 8;

    const size_t lo = median3(s, left, left + step, left + 2 * step, cmp);
    const size_t md = median3(s, mid - step, mid, mid + step, cmp);
    const size_t hi = median3(s, right - 2 * step, right - step, right, cmp);

    return median3(s, lo, md, hi, cmp);
}

static Split partition3(trs_SpanMut s, size_t left, size_t right, size_t pivot_idx, trs_Cmp cmp) {
    assert(left <= pivot_idx && pivot_idx <= right);

    trs_span_swap_elems(s, left, pivot_idx);

    const trs_Span v = trs_span_mut_to_span(s);

    size_t lo = left;
    size_t i = left + 1;
    size_t hi = right;

    // invariant: [left, lo-1] < pivot, [lo, i-1] == pivot, [i, hi] untouched,
    // [hi+1, right] > pivot. The equal run always holds at least the pivot elem, so
    // s[lo] is a copy of the pivot value and can be read instead of buffering it —
    // which matters here, where the elem size is only known at runtime.
    while (i <= hi) {
        const int c = cmp(trs_span_get(v, i), trs_span_get(v, lo));

        if (c < 0) {
            trs_span_swap_elems(s, i, lo);
            ++lo;
            ++i;
        } else if (c > 0) {
            trs_span_swap_elems(s, i, hi);
            --hi; // cannot wrap: the loop stops before hi reaches left
        } else {
            ++i;
        }
    }

    return (Split){.lt = lo, .gt = hi};
}

static void quicksort(trs_SpanMut s, size_t left, size_t right, trs_Cmp cmp) {
    while (left < right) {
        if (right - left + 1 <= INSERTION_THRESHOLD) {
            trs_span_insertion_sort(trs_span_sub_mut(s, left, right - left + 1), cmp);
            return;
        }

        // sampled across the range, not just at its two ends and middle: partitioning
        // leaves the smallest elem of the left side sitting at that side's last
        // position, and median-of-3 would then keep picking a near-minimum pivot
        const size_t pivot_idx = ninther(trs_span_mut_to_span(s), left, right, cmp);

        // three-way, so a run of equal keys is settled in one pass. A two-way split
        // peels those off one elem at a time, which is quadratic on repeated keys.
        const Split p = partition3(s, left, right, pivot_idx, cmp);
        const size_t lt = p.lt;
        const size_t gt = p.gt;

        // recurse into the shorter side and loop on the longer one: the recursion then
        // halves its range every time, so the stack stays O(log n) whatever the data
        if (lt - left < right - gt) {
            if (lt > left) {
                quicksort(s, left, lt - 1, cmp);
            }
            left = gt + 1;
        } else {
            if (gt < right) {
                quicksort(s, gt + 1, right, cmp);
            }
            if (lt == left) {
                return; // nothing sits below the pivot run
            }
            right = lt - 1;
        }
    }
}
