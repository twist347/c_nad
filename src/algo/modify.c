#include "nad/algo/modify.h"

#include <assert.h>
#include <string.h>

/* ========== internals ========== */

// lets the key-taking forms reuse the predicate-taking ones: the key and
// its equality travel together as the predicate's ctx
typedef struct {
    const void *key;
    nad_Eq eq;
} KeyMatch;

[[nodiscard]] static
bool key_matches(const void *elem, void *ctx) {
    const KeyMatch *self = ctx;

    return self->eq(elem, self->key);
}

/* ========== unique ========== */

size_t nad_span_unique(nad_SpanMut s, nad_Eq eq) {
    NAD_SPAN_ASSERT(s);
    assert(eq);

    if (s.len < 2) {
        return s.len;
    }

    const nad_Span view = nad_span_mut_to_span(s);
    size_t write = 1;

    for (size_t read = 1; read < s.len; ++read) {
        const void *cur = nad_span_get(view, read);

        // compared against the last elem KEPT, not the previous one read —
        // otherwise a run whose head was dropped would compare against a
        // value that is no longer there
        if (!eq(nad_span_get(view, write - 1), cur)) {
            if (write != read) {
                memcpy(nad_span_get_mut(s, write), cur, s.elem_size);
            }
            ++write;
        }
    }

    return write;
}

/* ========== remove ========== */

size_t nad_span_remove(nad_SpanMut s, const void *key, nad_Eq eq) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(eq);

    KeyMatch match = {.key = key, .eq = eq};

    return nad_span_remove_if(s, key_matches, &match);
}

size_t nad_span_remove_if(nad_SpanMut s, nad_Pred pred, void *ctx) {
    NAD_SPAN_ASSERT(s);
    assert(pred);

    const nad_Span view = nad_span_mut_to_span(s);
    size_t write = 0;

    for (size_t read = 0; read < s.len; ++read) {
        const void *cur = nad_span_get(view, read);

        if (!pred(cur, ctx)) {
            if (write != read) {
                memcpy(nad_span_get_mut(s, write), cur, s.elem_size);
            }
            ++write;
        }
    }

    return write;
}

/* ========== replace ========== */

void nad_span_replace(nad_SpanMut s, const void *key, const void *val, nad_Eq eq) {
    NAD_SPAN_ASSERT(s);
    assert(key);
    assert(val);
    assert(eq);

    KeyMatch match = {.key = key, .eq = eq};

    nad_span_replace_if(s, key_matches, &match, val);
}

void nad_span_replace_if(nad_SpanMut s, nad_Pred pred, void *ctx, const void *val) {
    NAD_SPAN_ASSERT(s);
    assert(pred);
    assert(val);

    const nad_Span view = nad_span_mut_to_span(s);

    for (size_t i = 0; i < s.len; ++i) {
        if (pred(nad_span_get(view, i), ctx)) {
            memcpy(nad_span_get_mut(s, i), val, s.elem_size);
        }
    }
}
