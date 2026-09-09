#include "terse/algo/search.h"

#include <assert.h>

/* ========== private decls ========== */

/// whether 'sub' sits in 's' starting at 'at'. The caller guarantees the room
static bool matches_at(trs_Span s, trs_Span sub, size_t at, trs_Eq eq);

/* ========== find ========== */

bool trs_span_find(trs_Span s, const void *key, trs_Eq eq, size_t *out_idx) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(eq);
    assert(out_idx);

    for (size_t i = 0; i < s.len; ++i) {
        if (eq(trs_span_get(s, i), key)) {
            *out_idx = i;
            return true;
        }
    }
    return false;
}

bool trs_span_find_if(trs_Span s, trs_Pred pred, void *ctx, size_t *out_idx) {
    TRS_SPAN_ASSERT(s);
    assert(pred);
    assert(out_idx);

    for (size_t i = 0; i < s.len; ++i) {
        if (pred(trs_span_get(s, i), ctx)) {
            *out_idx = i;
            return true;
        }
    }
    return false;
}

bool trs_span_find_sub(trs_Span s, trs_Span sub, trs_Eq eq, size_t *out_idx) {
    TRS_SPAN_ASSERT(s);
    TRS_SPAN_ASSERT(sub);
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

bool trs_span_find_sub_last(trs_Span s, trs_Span sub, trs_Eq eq, size_t *out_idx) {
    TRS_SPAN_ASSERT(s);
    TRS_SPAN_ASSERT(sub);
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

bool trs_span_find_run(trs_Span s, const void *key, size_t count, trs_Eq eq,
                       size_t *out_idx) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(eq);
    assert(out_idx);

    if (count == 0) {
        *out_idx = 0;
        return true;
    }

    size_t run = 0;
    for (size_t i = 0; i < s.len; ++i) {
        run = eq(trs_span_get(s, i), key) ? run + 1 : 0;

        if (run == count) {
            *out_idx = i + 1 - count;
            return true;
        }
    }
    return false;
}

bool trs_span_find_any_of(trs_Span s, trs_Span set, trs_Eq eq, size_t *out_idx) {
    TRS_SPAN_ASSERT(s);
    TRS_SPAN_ASSERT(set);
    assert(s.elem_size == set.elem_size);
    assert(eq);
    assert(out_idx);

    for (size_t i = 0; i < s.len; ++i) {
        const void *elem = trs_span_get(s, i);

        for (size_t j = 0; j < set.len; ++j) {
            if (eq(elem, trs_span_get(set, j))) {
                *out_idx = i;
                return true;
            }
        }
    }
    return false;
}

bool trs_span_find_adjacent(trs_Span s, trs_Eq eq, size_t *out_idx) {
    TRS_SPAN_ASSERT(s);
    assert(eq);
    assert(out_idx);

    // starts at 1 so that an empty span has nothing to compare rather than
    // s.len - 1 wrapping around
    for (size_t i = 1; i < s.len; ++i) {
        if (eq(trs_span_get(s, i - 1), trs_span_get(s, i))) {
            *out_idx = i - 1;
            return true;
        }
    }
    return false;
}

bool trs_span_contains(trs_Span s, const void *key, trs_Eq eq) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(eq);

    size_t idx;
    return trs_span_find(s, key, eq, &idx);
}

/* ========== count ========== */

size_t trs_span_count(trs_Span s, const void *key, trs_Eq eq) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(eq);

    size_t count = 0;
    for (size_t i = 0; i < s.len; ++i) {
        if (eq(trs_span_get(s, i), key)) {
            ++count;
        }
    }
    return count;
}

size_t trs_span_count_if(trs_Span s, trs_Pred pred, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(pred);

    size_t count = 0;
    for (size_t i = 0; i < s.len; ++i) {
        if (pred(trs_span_get(s, i), ctx)) {
            ++count;
        }
    }
    return count;
}

/* ========== binary search ========== */

size_t trs_span_lower_bound(trs_Span s, const void *key, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(cmp);

    size_t lo = 0, hi = s.len;

    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const void *midp = trs_span_get(s, mid);

        if (cmp(midp, key) < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}

size_t trs_span_upper_bound(trs_Span s, const void *key, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(cmp);

    size_t lo = 0, hi = s.len;

    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const void *midp = trs_span_get(s, mid);

        if (cmp(midp, key) <= 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}

bool trs_span_binary_search(trs_Span s, const void *key, trs_Cmp cmp, size_t *out_idx) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(cmp);
    assert(out_idx);

    const size_t pos = trs_span_lower_bound(s, key, cmp);
    if (pos >= s.len) {
        return false;
    }

    const void *p = trs_span_get(s, pos);
    if (cmp(p, key) == 0) {
        *out_idx = pos;
        return true;
    }
    return false;
}

trs_Range trs_span_equal_range(trs_Span s, const void *key, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(cmp);

    const size_t lo = trs_span_lower_bound(s, key, cmp);

    const trs_Span tail = trs_span_sub(s, lo, s.len - lo);

    return (trs_Range){
        .lo = lo,
        .hi = lo + trs_span_upper_bound(tail, key, cmp)
    };
}

size_t trs_span_partition_point(trs_Span s, trs_Pred pred, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(pred);

    size_t lo = 0, hi = s.len;

    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;

        if (pred(trs_span_get(s, mid), ctx)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}

/* ========== predicates ========== */

bool trs_span_all_of(trs_Span s, trs_Pred pred, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(pred);

    for (size_t i = 0; i < s.len; ++i) {
        if (!pred(trs_span_get(s, i), ctx)) {
            return false;
        }
    }
    return true;
}

bool trs_span_any_of(trs_Span s, trs_Pred pred, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(pred);

    size_t idx;
    return trs_span_find_if(s, pred, ctx, &idx);
}

bool trs_span_none_of(trs_Span s, trs_Pred pred, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(pred);

    return !trs_span_any_of(s, pred, ctx);
}

/* ========== extremes ========== */

size_t trs_span_min_elem(trs_Span s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(s.len > 0);

    size_t best = 0;
    const void *best_p = trs_span_get(s, 0);

    for (size_t i = 1; i < s.len; ++i) {
        const void *cur = trs_span_get(s, i);
        if (cmp(cur, best_p) < 0) {
            best = i;
            best_p = cur;
        }
    }

    return best;
}

size_t trs_span_max_elem(trs_Span s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(s.len > 0);

    size_t best = 0;
    const void *best_p = trs_span_get(s, 0);

    for (size_t i = 1; i < s.len; ++i) {
        const void *cur = trs_span_get(s, i);
        if (cmp(cur, best_p) > 0) {
            best = i;
            best_p = cur;
        }
    }

    return best;
}

trs_MinMax trs_span_minmax_elem(trs_Span s, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(s.len > 0);

    trs_MinMax out = {.min = 0, .max = 0};
    const void *min_p = trs_span_get(s, 0);
    const void *max_p = min_p;

    for (size_t i = 1; i < s.len; ++i) {
        const void *cur = trs_span_get(s, i);

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

static bool matches_at(trs_Span s, trs_Span sub, size_t at, trs_Eq eq) {
    assert(at + sub.len <= s.len);

    for (size_t j = 0; j < sub.len; ++j) {
        if (!eq(trs_span_get(s, at + j), trs_span_get(sub, j))) {
            return false;
        }
    }
    return true;
}
