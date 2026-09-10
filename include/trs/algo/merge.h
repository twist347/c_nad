#pragma once

#include "trs/alloc/alloc.h"
#include "trs/core/cmp.h"
#include "trs/core/export.h"
#include "trs/core/span.h"

/// @file

/// @defgroup algo_merge algo/merge
/// @ingroup algo
/// @brief joining two sorted runs into one
///
/// Both are stable: equal elems keep the run they came from, the left run first. A partly
/// sorted span is therefore sorted by finding the boundary with trs_span_is_sorted_until
/// and merging.
///
/// @par Example
/// @snippet algo/example_merge.c merge
/// @{

/// @name merge
/// @{

/// writes two sorted spans into 'dst' as one sorted run
/// @param dst where it goes; must hold a.len + b.len and overlap neither source
/// @param a one sorted run
/// @param b the other, sorted by the same 'cmp'
/// @param cmp the order both are in
/// @bigo{n} — one linear pass
TRS_API
void trs_span_merge(trs_SpanMut dst, trs_Span a, trs_Span b, trs_Cmp cmp);

/// joins the two sorted runs s[0, mid) and s[mid, len) in place
/// @param s the span; both halves must be sorted by 'cmp'
/// @param mid where the second run starts
/// @param cmp the order both are in
/// @param al where a buffer may come from, or null. Given one, it copies the shorter run
///           aside — never more than half — and merges back; without one, or on a refusal,
///           it takes a path that needs no memory
/// @bigo{n} with a buffer, O(n log n) without — a refusal is a slower way to the same
///        answer, not an error, which is why nothing is returned
TRS_API
void trs_span_inplace_merge(trs_SpanMut s, size_t mid, trs_Cmp cmp, trs_Al *al);

/// @}

/// @}
