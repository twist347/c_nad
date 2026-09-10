#include "trs/algo/modify.h"

#include <assert.h>
#include <string.h>

/* ========== internals ========== */

// lets the key-taking forms reuse the predicate-taking ones: the key and
// its equality travel together as the predicate's ctx
typedef struct {
    const void *key;
    trs_Eq eq;
} KeyMatch;

[[nodiscard]]
static bool key_matches(const void *elem, void *ctx) {
    const KeyMatch *self = ctx;

    return self->eq(elem, self->key);
}

/* ========== unique ========== */

size_t trs_span_unique(trs_SpanMut s, trs_Eq eq) {
    TRS_SPAN_ASSERT(s);
    assert(eq);

    if (s.len < 2) {
        return s.len;
    }

    const trs_Span view = trs_span_mut_to_span(s);
    size_t write = 1;

    for (size_t read = 1; read < s.len; ++read) {
        const void *cur = trs_span_get(view, read);

        // compared against the last elem KEPT, not the previous one read —
        // otherwise a run whose head was dropped would compare against a
        // value that is no longer there
        if (!eq(trs_span_get(view, write - 1), cur)) {
            if (write != read) {
                memcpy(trs_span_get_mut(s, write), cur, s.elem_size);
            }
            ++write;
        }
    }

    return write;
}

/* ========== remove ========== */

size_t trs_span_remove(trs_SpanMut s, const void *key, trs_Eq eq) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(eq);

    KeyMatch match = {.key = key, .eq = eq};

    return trs_span_remove_if(s, key_matches, &match);
}

size_t trs_span_remove_if(trs_SpanMut s, trs_Pred pred, void *ctx) {
    TRS_SPAN_ASSERT(s);
    assert(pred);

    const trs_Span view = trs_span_mut_to_span(s);
    size_t write = 0;

    for (size_t read = 0; read < s.len; ++read) {
        const void *cur = trs_span_get(view, read);

        if (!pred(cur, ctx)) {
            if (write != read) {
                memcpy(trs_span_get_mut(s, write), cur, s.elem_size);
            }
            ++write;
        }
    }

    return write;
}

/* ========== replace ========== */

void trs_span_replace(trs_SpanMut s, const void *key, const void *val, trs_Eq eq) {
    TRS_SPAN_ASSERT(s);
    assert(key);
    assert(val);
    assert(eq);

    KeyMatch match = {.key = key, .eq = eq};

    trs_span_replace_if(s, key_matches, &match, val);
}

void trs_span_replace_if(trs_SpanMut s, trs_Pred pred, void *ctx, const void *val) {
    TRS_SPAN_ASSERT(s);
    assert(pred);
    assert(val);

    const trs_Span view = trs_span_mut_to_span(s);

    for (size_t i = 0; i < s.len; ++i) {
        if (pred(trs_span_get(view, i), ctx)) {
            memcpy(trs_span_get_mut(s, i), val, s.elem_size);
        }
    }
}
