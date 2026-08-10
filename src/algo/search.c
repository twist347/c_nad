#include "nad/algo/search.h"

#include <assert.h>

/* ========== search ========== */

bool nad_span_find(nad_Span s, const void *key, nad_eq_fn eq, size_t *out_idx) {
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

bool nad_span_contains(nad_Span s, const void *key, nad_eq_fn eq) {
    size_t idx;
    return nad_span_find(s, key, eq, &idx);
}

size_t nad_span_count(nad_Span s, const void *key, nad_eq_fn eq) {
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

size_t nad_span_lower_bound(nad_Span s, const void *key, nad_cmp_fn cmp) {
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

size_t nad_span_upper_bound(nad_Span s, const void *key, nad_cmp_fn cmp) {
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

bool nad_span_bsearch(nad_Span s, const void *key, nad_cmp_fn cmp, size_t *out_idx) {
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

/* ========== extremes ========== */

size_t nad_span_min_elem(nad_Span s, nad_cmp_fn cmp) {
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

size_t nad_span_max_elem(nad_Span s, nad_cmp_fn cmp) {
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
