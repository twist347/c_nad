#include "nad/core/util.h"
#include "nad/algo/sort.h"
#include "nad/algo/copy.h"
#include "nad/algo/merge.h"

#include <assert.h>
#include <stdlib.h>

/* ========== internals ========== */

[[nodiscard]] static
size_t median3(nad_Span s, size_t a, size_t b, size_t c, nad_cmp_fn cmp);

[[nodiscard]] static
size_t partition_lomuto(nad_SpanMut s, size_t left, size_t right, size_t pivot_idx, nad_cmp_fn cmp);

/* ========== sort ========== */

void nad_span_insertion_sort(nad_SpanMut s, nad_cmp_fn cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);

    const nad_Span cs = nad_span_from_mut(s);
    for (size_t i = 1; i < s.len; ++i) {
        for (size_t j = i; j > 0; --j) {
            const void *prev = nad_span_get(cs, j - 1);
            const void *cur = nad_span_get(cs, j);

            if (cmp(prev, cur) <= 0) {
                break;
            }
            nad_span_swap_elems(s, j - 1, j);
        }
    }
}

void nad_span_sort(nad_SpanMut s, nad_cmp_fn cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);

    if (s.len < 2) {
        return;
    }

    qsort(s.data, s.len, s.elem_size, cmp);
}

nad_Status nad_span_sort_stable(nad_SpanMut s, nad_cmp_fn cmp, nad_Al *al) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);
    assert(al);

    if (s.len < 2) {
        return NAD_STATUS_OK;
    }

    const size_t bytes = s.len * s.elem_size;

    void *buf = nad_alloc(al, bytes);
    if (!buf) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    nad_SpanMut src = s;
    nad_SpanMut dst = nad_span_new_mut(buf, s.len, s.elem_size);

    // bottom-up merge sort: merge runs of width 1, 2, 4, ...
    for (size_t width = 1; width < s.len; width *= 2) {
        for (size_t i = 0; i < s.len; i += 2 * width) {
            const size_t mid = i + width < s.len ? i + width : s.len;
            const size_t end = i + 2 * width < s.len ? i + 2 * width : s.len;

            const nad_Span run = nad_span_from_mut(src);
            nad_span_merge(
                nad_span_sub_mut(dst, i, end - i),
                nad_span_sub(run, i, mid - i),
                nad_span_sub(run, mid, end - mid),
                cmp
            );
        }
        NAD_SWAP(src, dst);
    }

    // if result ended up in buf, copy back to original
    if (src.data != s.data) {
        nad_span_copy(s, nad_span_from_mut(src));
    }

    nad_dealloc(al, buf, bytes);

    return NAD_STATUS_OK;
}

void nad_span_partial_sort(nad_SpanMut s, size_t n, nad_cmp_fn cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);
    assert(n <= s.len);

    if (n == 0 || s.len < 2) {
        return;
    }

    if (n >= s.len) {
        nad_span_sort(s, cmp);
        return;
    }

    // place element that would be at position k in sorted order
    nad_span_nth_elem(s, n, cmp);

    // now the first k elements are the k smallest (order unspecified) -> sort them
    nad_span_sort(nad_span_sub_mut(s, 0, n), cmp);
}

void nad_span_nth_elem(nad_SpanMut s, size_t nth, nad_cmp_fn cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);
    assert(nth < s.len);

    if (s.len < 2) {
        return;
    }

    size_t left = 0, right = s.len - 1;

    while (left < right) {
        const size_t mid = left + (right - left) / 2;
        const size_t pivot_index = median3(nad_span_from_mut(s), left, mid, right, cmp);

        const size_t p = partition_lomuto(s, left, right, pivot_index, cmp);

        if (nth == p) {
            return;
        }
        if (nth < p) {
            // p is > 0 here because nth < p and nth is usize
            right = p - 1;
        } else {
            left = p + 1;
        }
    }
}

/* ========== info ========== */

bool nad_span_is_sorted(nad_Span s, nad_cmp_fn cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);

    for (size_t i = 1; i < s.len; ++i) {
        const void *prev = nad_span_get(s, i - 1);
        const void *cur = nad_span_get(s, i);
        if (cmp(prev, cur) > 0) {
            return false;
        }
    }
    return true;
}

/* ========== internals ========== */

static
size_t median3(nad_Span s, size_t a, size_t b, size_t c, nad_cmp_fn cmp) {
    const void *a_ptr = nad_span_get(s, a);
    const void *b_ptr = nad_span_get(s, b);
    const void *c_ptr = nad_span_get(s, c);

    const int ab = cmp(a_ptr, b_ptr);
    const int ac = cmp(a_ptr, c_ptr);
    const int bc = cmp(b_ptr, c_ptr);

    // A between B and C
    if ((ab <= 0 && ac >= 0) || (ab >= 0 && ac <= 0)) return a;
    // B between A and C
    if ((ab >= 0 && bc <= 0) || (ab <= 0 && bc >= 0)) return b;
    // else C is median
    return c;
}

static
size_t partition_lomuto(nad_SpanMut s, size_t left, size_t right, size_t pivot_idx, nad_cmp_fn cmp) {
    assert(left <= pivot_idx && pivot_idx <= right);

    nad_span_swap_elems(s, pivot_idx, right);
    const void *pivot = nad_span_get(nad_span_from_mut(s), right);

    size_t i = left;
    for (size_t j = left; j < right; ++j) {
        const void *x = nad_span_get(nad_span_from_mut(s), j);
        if (cmp(x, pivot) < 0) {
            nad_span_swap_elems(s, i, j);
            ++i;
        }
    }

    nad_span_swap_elems(s, i, right);
    return i;
}

