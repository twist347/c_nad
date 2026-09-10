#include "trs/algo/set.h"

#include "internal/emit.h"

#include <assert.h>

/* ========== set ops ========== */

size_t trs_span_set_union(trs_SpanMut dst, trs_Span a, trs_Span b, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len >= a.len + b.len);

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = trs_span_get(a, i);
        const void *r = trs_span_get(b, j);
        const int c = cmp(l, r);

        if (c < 0) {
            out = trs_emit(dst, out, l);
            ++i;
        } else if (c > 0) {
            out = trs_emit(dst, out, r);
            ++j;
        } else {
            // one copy of the pair, taken from 'a'; a run left over on either side is
            // picked up by the turns that follow, which is what makes the count max(m, n)
            out = trs_emit(dst, out, l);
            ++i;
            ++j;
        }
    }

    out = trs_emit_rest(dst, out, a, i);
    out = trs_emit_rest(dst, out, b, j);

    return out;
}

size_t trs_span_set_intersection(trs_SpanMut dst, trs_Span a, trs_Span b, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len >= (a.len < b.len ? a.len : b.len));

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = trs_span_get(a, i);
        const void *r = trs_span_get(b, j);
        const int c = cmp(l, r);

        if (c < 0) {
            ++i;
        } else if (c > 0) {
            ++j;
        } else {
            // the copy handed out is 'a's: equal by 'cmp' does not mean identical, so
            // which side an elem comes from is observable and has to be a decision
            out = trs_emit(dst, out, l);
            ++i;
            ++j;
        }
    }

    return out;
}

size_t trs_span_set_difference(trs_SpanMut dst, trs_Span a, trs_Span b, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len >= a.len);

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = trs_span_get(a, i);
        const void *r = trs_span_get(b, j);
        const int c = cmp(l, r);

        if (c < 0) {
            out = trs_emit(dst, out, l);
            ++i;
        } else if (c > 0) {
            ++j;
        } else {
            // one copy of 'a' spent against one copy of 'b'
            ++i;
            ++j;
        }
    }

    // whatever is left in 'b' cancels nothing: it has no counterpart left in 'a'
    return trs_emit_rest(dst, out, a, i);
}

size_t trs_span_set_symmetric_difference(trs_SpanMut dst, trs_Span a, trs_Span b, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(dst);
    TRS_SPAN_ASSERT(a);
    TRS_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len >= a.len + b.len);

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = trs_span_get(a, i);
        const void *r = trs_span_get(b, j);
        const int c = cmp(l, r);

        if (c < 0) {
            out = trs_emit(dst, out, l);
            ++i;
        } else if (c > 0) {
            out = trs_emit(dst, out, r);
            ++j;
        } else {
            // the pair cancels; a longer run on one side survives by this same rule on
            // the turns that follow, which is what makes the count |m - n|
            ++i;
            ++j;
        }
    }

    out = trs_emit_rest(dst, out, a, i);
    out = trs_emit_rest(dst, out, b, j);

    return out;
}

/* ========== predicates ========== */

bool trs_span_includes(trs_Span sup, trs_Span sub, trs_Cmp cmp) {
    TRS_SPAN_ASSERT(sup);
    TRS_SPAN_ASSERT(sub);
    assert(cmp);
    assert(sup.elem_size == sub.elem_size);

    size_t i = 0;

    for (size_t j = 0; j < sub.len; ++j) {
        const void *want = trs_span_get(sub, j);

        // walk 'sup' up to the elem being accounted for; running out of it, or stepping
        // past the value, both mean this copy has no match left
        while (i < sup.len && cmp(trs_span_get(sup, i), want) < 0) {
            ++i;
        }

        if (i == sup.len || cmp(trs_span_get(sup, i), want) > 0) {
            return false;
        }

        // matched: that copy of it is spent, so duplicates need duplicates
        ++i;
    }

    return true;
}
