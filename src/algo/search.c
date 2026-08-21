#include "nad/algo/search.h"

#include <assert.h>

/* ========== private decls ========== */

/// whether 'sub' sits in 's' starting at 'at'. The caller guarantees the room
static bool matches_at(nad_Span s, nad_Span sub, size_t at, nad_Eq eq);

/* ========== find ========== */

bool nad_span_find(nad_Span s, const void *key, nad_Eq eq, size_t *out_idx) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(eq);
    assert(out_idx);

    for (size_t i = 0; i < s.len; ++i) {
        if (eq(nad_span_get(s, i), key)) {
            *out_idx = i;
            return true;
        }
    }
    return false;
}

bool nad_span_find_if(nad_Span s, nad_Pred pred, void *ctx, size_t *out_idx) {
    NAD_SPAN_ASSERT(s);
    assert(pred);
    assert(out_idx);

    for (size_t i = 0; i < s.len; ++i) {
        if (pred(nad_span_get(s, i), ctx)) {
            *out_idx = i;
            return true;
        }
    }
    return false;
}

bool nad_span_find_sub(nad_Span s, nad_Span sub, nad_Eq eq, size_t *out_idx) {
    NAD_SPAN_ASSERT(s);
    NAD_SPAN_ASSERT(sub);
    assert(s.elem_size == sub.elem_size);
    assert(eq);
    assert(out_idx);

    if (sub.len == 0) {
        *out_idx = 0;
        return true;
    }
    if (sub.len > s.len) {
        return false;
    }

    // s.len - sub.len is the last start that still leaves room for the whole sub,
    // and cannot wrap: the case sub.len > s.len is already out
    for (size_t i = 0; i + sub.len <= s.len; ++i) {
        if (matches_at(s, sub, i, eq)) {
            *out_idx = i;
            return true;
        }
    }
    return false;
}

bool nad_span_find_sub_last(nad_Span s, nad_Span sub, nad_Eq eq, size_t *out_idx) {
    NAD_SPAN_ASSERT(s);
    NAD_SPAN_ASSERT(sub);
    assert(s.elem_size == sub.elem_size);
    assert(eq);
    assert(out_idx);

    if (sub.len == 0) {
        *out_idx = s.len;
        return true;
    }
    if (sub.len > s.len) {
        return false;
    }

    // counts down through 0, so the loop var is the start plus one — a size_t
    // running below zero wraps instead of ending the loop
    for (size_t start = s.len - sub.len + 1; start > 0; --start) {
        if (matches_at(s, sub, start - 1, eq)) {
            *out_idx = start - 1;
            return true;
        }
    }
    return false;
}

bool nad_span_find_run(nad_Span s, const void *key, size_t n, nad_Eq eq, size_t *out_idx) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(eq);
    assert(out_idx);

    if (n == 0) {
        *out_idx = 0;
        return true;
    }

    size_t run = 0;
    for (size_t i = 0; i < s.len; ++i) {
        run = eq(nad_span_get(s, i), key) ? run + 1 : 0;

        if (run == n) {
            *out_idx = i + 1 - n;
            return true;
        }
    }
    return false;
}

bool nad_span_find_any_of(nad_Span s, nad_Span set, nad_Eq eq, size_t *out_idx) {
    NAD_SPAN_ASSERT(s);
    NAD_SPAN_ASSERT(set);
    assert(s.elem_size == set.elem_size);
    assert(eq);
    assert(out_idx);

    for (size_t i = 0; i < s.len; ++i) {
        const void *elem = nad_span_get(s, i);

        for (size_t j = 0; j < set.len; ++j) {
            if (eq(elem, nad_span_get(set, j))) {
                *out_idx = i;
                return true;
            }
        }
    }
    return false;
}

bool nad_span_find_adjacent(nad_Span s, nad_Eq eq, size_t *out_idx) {
    NAD_SPAN_ASSERT(s);
    assert(eq);
    assert(out_idx);

    // starts at 1 so that an empty span has nothing to compare rather than
    // s.len - 1 wrapping around
    for (size_t i = 1; i < s.len; ++i) {
        if (eq(nad_span_get(s, i - 1), nad_span_get(s, i))) {
            *out_idx = i - 1;
            return true;
        }
    }
    return false;
}

bool nad_span_contains(nad_Span s, const void *key, nad_Eq eq) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(eq);

    size_t idx;
    return nad_span_find(s, key, eq, &idx);
}

/* ========== count ========== */

size_t nad_span_count(nad_Span s, const void *key, nad_Eq eq) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(eq);

    size_t count = 0;
    for (size_t i = 0; i < s.len; ++i) {
        if (eq(nad_span_get(s, i), key)) {
            ++count;
        }
    }
    return count;
}

size_t nad_span_count_if(nad_Span s, nad_Pred pred, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(pred);

    size_t count = 0;
    for (size_t i = 0; i < s.len; ++i) {
        if (pred(nad_span_get(s, i), ctx)) {
            ++count;
        }
    }
    return count;
}

/* ========== binary search ========== */

size_t nad_span_lower_bound(nad_Span s, const void *key, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(cmp);

    size_t lo = 0, hi = s.len;

    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const void *midp = nad_span_get(s, mid);

        if (cmp(midp, key) < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}

size_t nad_span_upper_bound(nad_Span s, const void *key, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(cmp);

    size_t lo = 0, hi = s.len;

    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const void *midp = nad_span_get(s, mid);

        if (cmp(midp, key) <= 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}

bool nad_span_binary_search(nad_Span s, const void *key, nad_Cmp cmp, size_t *out_idx) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(cmp);
    assert(out_idx);

    const size_t pos = nad_span_lower_bound(s, key, cmp);
    if (pos >= s.len) {
        return false;
    }

    const void *p = nad_span_get(s, pos);
    if (cmp(p, key) == 0) {
        *out_idx = pos;
        return true;
    }
    return false;
}

nad_Range nad_span_equal_range(nad_Span s, const void *key, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(cmp);

    const size_t lo = nad_span_lower_bound(s, key, cmp);

    const nad_Span tail = nad_span_sub(s, lo, s.len - lo);

    return (nad_Range){
        .lo = lo,
        .hi = lo + nad_span_upper_bound(tail, key, cmp)
    };
}

size_t nad_span_partition_point(nad_Span s, nad_Pred pred, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(pred);

    size_t lo = 0, hi = s.len;

    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;

        if (pred(nad_span_get(s, mid), ctx)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}

/* ========== predicates ========== */

bool nad_span_all_of(nad_Span s, nad_Pred pred, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(pred);

    for (size_t i = 0; i < s.len; ++i) {
        if (!pred(nad_span_get(s, i), ctx)) {
            return false;
        }
    }
    return true;
}

bool nad_span_any_of(nad_Span s, nad_Pred pred, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(pred);

    size_t idx;
    return nad_span_find_if(s, pred, ctx, &idx);
}

bool nad_span_none_of(nad_Span s, nad_Pred pred, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(pred);

    return !nad_span_any_of(s, pred, ctx);
}

/* ========== extremes ========== */

size_t nad_span_min_elem(nad_Span s, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);
    assert(s.len > 0);

    size_t best = 0;
    const void *best_p = nad_span_get(s, 0);

    for (size_t i = 1; i < s.len; ++i) {
        const void *cur = nad_span_get(s, i);
        if (cmp(cur, best_p) < 0) {
            best = i;
            best_p = cur;
        }
    }

    return best;
}

size_t nad_span_max_elem(nad_Span s, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);
    assert(s.len > 0);

    size_t best = 0;
    const void *best_p = nad_span_get(s, 0);

    for (size_t i = 1; i < s.len; ++i) {
        const void *cur = nad_span_get(s, i);
        if (cmp(cur, best_p) > 0) {
            best = i;
            best_p = cur;
        }
    }

    return best;
}

nad_MinMax nad_span_minmax_elem(nad_Span s, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);
    assert(s.len > 0);

    nad_MinMax out = {.min = 0, .max = 0};
    const void *min_p = nad_span_get(s, 0);
    const void *max_p = min_p;

    for (size_t i = 1; i < s.len; ++i) {
        const void *cur = nad_span_get(s, i);

        if (cmp(cur, min_p) < 0) {
            out.min = i;
            min_p = cur;
        }
        if (cmp(cur, max_p) > 0) {
            out.max = i;
            max_p = cur;
        }
    }

    return out;
}

/* ========== private defs ========== */

static bool matches_at(nad_Span s, nad_Span sub, size_t at, nad_Eq eq) {
    assert(at + sub.len <= s.len);

    for (size_t j = 0; j < sub.len; ++j) {
        if (!eq(nad_span_get(s, at + j), nad_span_get(sub, j))) {
            return false;
        }
    }
    return true;
}
