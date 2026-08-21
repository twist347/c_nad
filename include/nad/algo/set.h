#pragma once

#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

#include <stddef.h>

/*
 * Set operations over two spans that are already SORTED by the same 'cmp'. Sortedness is
 * the precondition that makes them linear: every one of these walks both spans once.
 *
 * These are MULTISET operations — equal elems are counted, not collapsed. When 'a' holds
 * m copies of a value and 'b' holds n, the result holds:
 *
 *     union                  max(m, n)      the m from 'a', then n - m more from 'b'
 *     intersection           min(m, n)      taken from 'a'
 *     difference             m - n, or 0    taken from 'a'
 *     symmetric difference   |m - n|        from whichever side has more
 *
 * Feed them spans through nad_span_unique first to get the plain set behaviour instead.
 *
 * Each writes into 'dst' and returns how many elems it wrote, exactly like
 * nad_span_copy_if: the length of a result is not known before it is computed, so 'dst'
 * has to be long enough for the worst case and the return value says how much of it is
 * filled. The worst case differs per operation and is written above each one.
 *
 * 'dst' must not overlap 'a' or 'b' — they write while they read.
 *
 * Every result comes out sorted by the same 'cmp', so these compose with each other and
 * with nad_span_merge without a sort in between.
 */

/* ========== set ops ========== */

/// every elem of both, keeping the greater count of the equal ones.
/// 'dst' must hold a.len + b.len
[[nodiscard]] NAD_API
size_t nad_span_set_union(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp);

/// the elems present in both. 'dst' must hold the shorter of the two
[[nodiscard]] NAD_API
size_t nad_span_set_intersection(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp);

/// the elems of 'a' that 'b' does not account for. 'dst' must hold a.len
[[nodiscard]] NAD_API
size_t nad_span_set_difference(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp);

/// the elems present in exactly one of the two. 'dst' must hold a.len + b.len
[[nodiscard]] NAD_API
size_t nad_span_set_symmetric_difference(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp);

/* ========== predicates ========== */

/// whether 'sup' accounts for every elem of 'sub', counting duplicates: the subset
/// test, with the containing span first. An empty 'sub' is included in anything
[[nodiscard]] NAD_API
bool nad_span_includes(nad_Span sup, nad_Span sub, nad_Cmp cmp);
