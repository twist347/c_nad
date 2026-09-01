#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

/// @file

/// @defgroup algo_sort algo/sort
/// @ingroup algo
/// @brief putting a span in order, and asking whether it already is
///
/// nad_span_sort is the default: a quicksort, ninther pivot, three-way split, in place
/// and allocating nothing. It is not stable — stability costs a buffer, so it is a
/// separate name that says so by taking an allocator and returning a nad_Status.
///
/// The rest answer less: partial_sort when only the first few matter, nth_elem when only
/// one position does, and the predicates that ask instead of doing.
///
/// @par Example
/// @snippet algo/example_sort.c sort
/// @snippet algo/example_sort.c partial
/// @{

/// @name sort
/// @{

/// sorts by insertion — quadratic, and worth it only on a span already nearly in order
/// @param s the span
/// @param cmp the order to put it in
/// @bigo{n^2} — O(n) on a sorted span, which is what it is for
NAD_API
void nad_span_insertion_sort(nad_SpanMut s, nad_Cmp cmp);

/// sorts in place, unstably
/// @param s the span
/// @param cmp the order to put it in
/// @bigo{n log n} — O(n) on a span of few distinct values, the split paying off
NAD_API
void nad_span_sort(nad_SpanMut s, nad_Cmp cmp);

/// sorts in place, keeping equal elems in the order they were
/// @param s the span
/// @param cmp the order to put it in
/// @param al where the buffer comes from; a bottom-up merge sort needs one span-sized,
///           which is the whole reason this returns a status
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when 'al' cannot give it; 's' is left as it was
/// @bigo{n log n}
[[nodiscard]] NAD_API
nad_Status nad_span_sort_stable(nad_SpanMut s, nad_Cmp cmp, nad_Al *al);

/// puts the smallest 'count' elems, in order, at the front
/// @param s the span
/// @param count how many of them; asserts count <= s.len
/// @param cmp the order to take them by
/// @bigo{n log k} — k is 'count'; the rest of the span is left in no particular order
NAD_API
void nad_span_partial_sort(nad_SpanMut s, size_t count, nad_Cmp cmp);

/// puts at 'nth' the elem that would be there in sorted order
/// @param s the span; asserts it is not empty
/// @param nth the position to settle; asserts nth < s.len. Before it nothing is greater,
///            after it nothing is less, and neither side is ordered
/// @param cmp the order in question
/// @bigo{n} expected — a quickselect
NAD_API
void nad_span_nth_elem(nad_SpanMut s, size_t nth, nad_Cmp cmp);

/// @}

/// @name predicates
/// @{

/// whether the span is in order
/// @param s the span; an empty one and a single elem are in order
/// @param cmp the order in question
/// @return whether no elem comes before the one ahead of it
/// @bigo{n} — it stops at the first pair out of order
[[nodiscard]] NAD_API
bool nad_span_is_sorted(nad_Span s, nad_Cmp cmp);

/// how far the span is in order
/// @param s the span
/// @param cmp the order in question
/// @return the index of the first elem out of order, or s.len when there is none. The
///         prefix before it is always sorted, which is what nad_span_inplace_merge wants
/// @bigo{n}
[[nodiscard]] NAD_API
size_t nad_span_is_sorted_until(nad_Span s, nad_Cmp cmp);

/// @}

/// @}
