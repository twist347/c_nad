#pragma once

#include "trs/core/cmp.h"
#include "trs/core/export.h"
#include "trs/core/span.h"

/// @file

/// @defgroup algo_heap algo/heap
/// @ingroup algo
/// @brief a max-heap laid out in the span itself
///
/// A max-heap by 'cmp': no elem compares greater than its parent, so the largest sits at
/// index 0. A min-heap is these same functions with a descending comparator — there is
/// deliberately no second set. The shape is the usual implicit tree, the children of i at
/// 2i+1 and 2i+2, so nothing is stored but the elems, in place and allocating nothing.
///
/// @par Example
/// @snippet algo/example_heap.c build
/// @snippet algo/example_heap.c push
/// @{

/// @name build
/// @{

/// rearranges the whole span into a heap
/// @param s the span
/// @param cmp the order the heap is by
/// @bigo{n} — bottom-up, not n insertions
TRS_API
void trs_span_make_heap(trs_SpanMut s, trs_Cmp cmp);

/// @}

/// @name push / pop
/// @{

/// sifts the last elem up into place, everything before it being a heap already
/// @param s the span; all but its last elem must be a heap by 'cmp'
/// @param cmp the order the heap is by
/// @bigo{log n} — pairs with trs_vec_push: append the elem, then this over the vec
TRS_API
void trs_span_push_heap(trs_SpanMut s, trs_Cmp cmp);

/// moves the largest to the last position and closes the heap over what precedes it —
/// the span is then no longer a heap, but its head is
/// @param s the span; must be a heap by 'cmp'
/// @param cmp the order the heap is by
/// @bigo{log n} — pairs with trs_vec_pop: this first, then drop the tail
TRS_API
void trs_span_pop_heap(trs_SpanMut s, trs_Cmp cmp);

/// @}

/// @name sort
/// @{

/// turns a heap into an ascending run, by repeated pop
/// @param s the span; must be a heap by 'cmp'
/// @param cmp the order the heap is by
/// @bigo{n log n} — heapsort is make_heap and then this, so it needs no name
TRS_API
void trs_span_sort_heap(trs_SpanMut s, trs_Cmp cmp);

/// @}

/// @name predicates
/// @{

/// whether the span is a heap
/// @param s the span; an empty one and a single elem are heaps
/// @param cmp the order in question
/// @return whether no elem compares greater than its parent
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_span_is_heap(trs_Span s, trs_Cmp cmp);

/// how far the span is a heap
/// @param s the span
/// @param cmp the order in question
/// @return the index of the first elem that breaks it, or s.len when none does; the
///         prefix before the answer is always a heap
/// @bigo{n}
[[nodiscard]] TRS_API
size_t trs_span_is_heap_until(trs_Span s, trs_Cmp cmp);

/// @}

/// @}
