#pragma once

#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

#include <stddef.h>

/// @file

/// @defgroup algo_set algo/set
/// @ingroup algo
/// @brief set operations over two spans already sorted by the same cmp
///
/// Sortedness is what makes them linear: each walks both spans once and comes out sorted
/// by the same cmp, so they compose with each other and with nad_span_merge unsorted.
///
/// These are multiset ops — equal elems are counted, not collapsed. Where 'a' holds m
/// copies of a value and 'b' holds n, the result holds:
///
///     union                  max(m, n)      the m from 'a', then n - m more from 'b'
///     intersection           min(m, n)      taken from 'a'
///     difference             m - n, or 0    taken from 'a'
///     symmetric difference   |m - n|        from whichever side has more
///
/// Run the spans through nad_span_unique first for plain set behaviour instead.
///
/// Each writes into 'dst' and returns how much was filled, as nad_span_copy_if does: a
/// result's length is not known before it is computed, so 'dst' must fit the worst case,
/// written on each one. It must overlap neither source — they write while they read.
///
/// @par Example
/// @snippet algo/example_set.c ops
/// @{

/// @name set ops
/// @{

/// every elem of both, keeping the greater count of the equal ones
/// @param dst where they go; must hold a.len + b.len
/// @param a one sorted span
/// @param b the other, sorted by the same 'cmp'
/// @param cmp the order both are in
/// @return how many elems were written
/// @bigo{n}
[[nodiscard]] NAD_API
size_t nad_span_set_union(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp);

/// the elems present in both
/// @param dst where they go; must hold the shorter of the two
/// @param a one sorted span
/// @param b the other, sorted by the same 'cmp'
/// @param cmp the order both are in
/// @return how many elems were written
/// @bigo{n}
[[nodiscard]] NAD_API
size_t nad_span_set_intersection(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp);

/// the elems of 'a' that 'b' does not account for
/// @param dst where they go; must hold a.len
/// @param a the span to take from
/// @param b the span to account against, sorted by the same 'cmp'
/// @param cmp the order both are in
/// @return how many elems were written
/// @bigo{n}
[[nodiscard]] NAD_API
size_t nad_span_set_difference(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp);

/// the elems present in exactly one of the two
/// @param dst where they go; must hold a.len + b.len
/// @param a one sorted span
/// @param b the other, sorted by the same 'cmp'
/// @param cmp the order both are in
/// @return how many elems were written
/// @bigo{n}
[[nodiscard]] NAD_API
size_t nad_span_set_symmetric_difference(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp);

/// @}

/// @name predicates
/// @{

/// the subset test, with the containing span first
/// @param sup the span that may account for the other
/// @param sub the span to be accounted for; an empty one is included in anything
/// @param cmp the order both are in
/// @return whether 'sup' accounts for every elem of 'sub', counting duplicates
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_span_includes(nad_Span sup, nad_Span sub, nad_Cmp cmp);

/// @}

/// @}
