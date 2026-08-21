#pragma once

#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

/// A heap here is a **max-heap by 'cmp'**: no elem compares greater than its parent, so
/// the largest sits at index 0. A min-heap is the same functions with a descending
/// comparator (nad_cmp_desc_i32 and friends) — there is deliberately no second set.
///
/// The shape is the usual implicit tree: the children of 'i' live at 2i+1 and 2i+2, so
/// nothing is stored but the elems themselves. Every op works in place and allocates
/// nothing.

/* ========== build ========== */

/// rearranges the whole span into a heap, in O(len)
NAD_API
void nad_span_make_heap(nad_SpanMut s, nad_Cmp cmp);

/* ========== push / pop ========== */

/// sifts the LAST elem of 's' up into place, treating everything before it as an
/// existing heap. Pairs with nad_vec_push: append the elem, then push_heap over the vec
NAD_API
void nad_span_push_heap(nad_SpanMut s, nad_Cmp cmp);

/// moves the largest elem to the LAST position and restores the heap over what precedes
/// it, so the span is no longer a heap but its head is. Pairs with nad_vec_pop: pop_heap
/// first, then drop the tail
NAD_API
void nad_span_pop_heap(nad_SpanMut s, nad_Cmp cmp);

/* ========== sort ========== */

/// turns a heap into an ascending run by repeated pop_heap; 's' must already be one.
/// Heapsort is therefore make_heap followed by sort_heap, and needs no name of its own
NAD_API
void nad_span_sort_heap(nad_SpanMut s, nad_Cmp cmp);

/* ========== predicates ========== */

[[nodiscard]] NAD_API
bool nad_span_is_heap(nad_Span s, nad_Cmp cmp);

/// index of the first elem that breaks the heap property, or 'len' when none does;
/// the prefix before the answer is always a heap
[[nodiscard]] NAD_API
size_t nad_span_is_heap_until(nad_Span s, nad_Cmp cmp);
