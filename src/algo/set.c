#include "nad/algo/set.h"

#include "internal/emit.h"

#include <assert.h>

/* ========== set ops ========== */

size_t nad_span_set_union(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len >= a.len + b.len);

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = nad_span_get(a, i);
        const void *r = nad_span_get(b, j);
        const int c = cmp(l, r);

        if (c < 0) {
            out = nad_emit(dst, out, l);
            ++i;
        } else if (c > 0) {
            out = nad_emit(dst, out, r);
            ++j;
        } else {
            // one copy of the pair, taken from 'a'; a run left over on either side is
            // picked up by the turns that follow, which is what makes the count max(m, n)
            out = nad_emit(dst, out, l);
            ++i;
            ++j;
        }
    }

    out = nad_emit_rest(dst, out, a, i);
    out = nad_emit_rest(dst, out, b, j);

    return out;
}

size_t nad_span_set_intersection(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len >= (a.len < b.len ? a.len : b.len));

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = nad_span_get(a, i);
        const void *r = nad_span_get(b, j);
        const int c = cmp(l, r);

        if (c < 0) {
            ++i;
        } else if (c > 0) {
            ++j;
        } else {
            // the copy handed out is 'a's: equal by 'cmp' does not mean identical, so
            // which side an elem comes from is observable and has to be a decision
            out = nad_emit(dst, out, l);
            ++i;
            ++j;
        }
    }

    return out;
}

size_t nad_span_set_difference(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len >= a.len);

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = nad_span_get(a, i);
        const void *r = nad_span_get(b, j);
        const int c = cmp(l, r);

        if (c < 0) {
            out = nad_emit(dst, out, l);
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
    return nad_emit_rest(dst, out, a, i);
}

size_t nad_span_set_symmetric_difference(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(dst);
    NAD_SPAN_ASSERT(a);
    NAD_SPAN_ASSERT(b);
    assert(cmp);
    assert(dst.elem_size == a.elem_size);
    assert(dst.elem_size == b.elem_size);
    assert(dst.len >= a.len + b.len);

    size_t i = 0, j = 0;
    size_t out = 0;

    while (i < a.len && j < b.len) {
        const void *l = nad_span_get(a, i);
        const void *r = nad_span_get(b, j);
        const int c = cmp(l, r);

        if (c < 0) {
            out = nad_emit(dst, out, l);
            ++i;
        } else if (c > 0) {
            out = nad_emit(dst, out, r);
            ++j;
        } else {
            // the pair cancels; a longer run on one side survives by this same rule on
            // the turns that follow, which is what makes the count |m - n|
            ++i;
            ++j;
        }
    }

    out = nad_emit_rest(dst, out, a, i);
    out = nad_emit_rest(dst, out, b, j);

    return out;
}

/* ========== predicates ========== */

bool nad_span_includes(nad_Span sup, nad_Span sub, nad_Cmp cmp) {
    NAD_SPAN_ASSERT(sup);
    NAD_SPAN_ASSERT(sub);
    assert(cmp);
    assert(sup.elem_size == sub.elem_size);

    size_t i = 0;

    for (size_t j = 0; j < sub.len; ++j) {
        const void *want = nad_span_get(sub, j);

        // walk 'sup' up to the elem being accounted for; running out of it, or stepping
        // past the value, both mean this copy has no match left
        while (i < sup.len && cmp(nad_span_get(sup, i), want) < 0) {
            ++i;
        }

        if (i == sup.len || cmp(nad_span_get(sup, i), want) > 0) {
            return false;
        }

        // matched: that copy of it is spent, so duplicates need duplicates
        ++i;
    }

    return true;
}
