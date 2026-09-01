#pragma once

#include "nad/algo/fn.h"
#include "nad/alloc/alloc.h"
#include "nad/core/export.h"
#include "nad/core/cmp.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// @file

/// @defgroup algo_permute algo/permute
/// @ingroup algo
/// @brief rearranging a span without changing what it holds
///
/// All in place and allocating nothing — except the one that keeps an order, which says
/// so by taking an allocator and returning a nad_Status.
///
/// @par Example
/// @snippet algo/example_permute.c pred
/// @snippet algo/example_permute.c move
/// @snippet algo/example_permute.c partition
/// @{

/// @name move
/// @{

/// turns the span back to front
/// @param s the span
/// @bigo{n}
NAD_API
void nad_span_reverse(nad_SpanMut s);

/// rotates left so that the elem at 'mid' becomes the first
/// @param s the span
/// @param mid the new front; asserts mid <= s.len, and 0 or s.len is a no-op
/// @bigo{n} — three reversals, no buffer
NAD_API
void nad_span_rotate(nad_SpanMut s, size_t mid);

/// exchanges the elems of two spans, position by position
/// @param a one span
/// @param b the other; asserts the same len and elem_size
/// @bigo{n}
NAD_API
void nad_span_swap_ranges(nad_SpanMut a, nad_SpanMut b);

/// @}

/// @name permutations
/// @{

/// rearranges into the next permutation in 'cmp' order
/// @param s the span
/// @param cmp the order permutations are counted in
/// @return whether there was one; on the last it wraps to the first and returns false,
///         so a do-while over a sorted span walks them all exactly once
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_span_next_permutation(nad_SpanMut s, nad_Cmp cmp);

/// rearranges into the previous permutation in 'cmp' order
/// @param s the span
/// @param cmp the order permutations are counted in
/// @return whether there was a previous one; on the first it wraps to the last
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_span_prev_permutation(nad_SpanMut s, nad_Cmp cmp);

/// @}

/// @name partition
/// @{

/// moves the elems satisfying 'pred' to the front
/// @param s the span
/// @param pred the test
/// @param ctx handed to 'pred'
/// @return where they end — the index of the first elem that does not satisfy 'pred'
/// @bigo{n} — it swaps, so neither side keeps the order it had
[[nodiscard]] NAD_API
size_t nad_span_partition(nad_SpanMut s, nad_Pred pred, void *ctx);

/// the same split, both sides keeping the order they had
/// @param s the span
/// @param pred the test, asked about each elem once
/// @param ctx handed to 'pred'
/// @param al where the scratch comes from; an order costs one span-sized, as in
///           nad_span_sort_stable
/// @param[out] out_boundary where the front part ends, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when 'al' cannot give the buffer; 's' is untouched
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Status nad_span_partition_stable(
    nad_SpanMut s,
    nad_Pred pred,
    void *ctx,
    nad_Al *al,
    size_t *out_boundary
);

/// whether the span is already split that way
/// @param s the span
/// @param pred the test
/// @param ctx handed to 'pred'
/// @return whether every elem satisfying 'pred' comes before every elem that does not
/// @bigo{n}
[[nodiscard]] NAD_API
bool nad_span_is_partitioned(nad_Span s, nad_Pred pred, void *ctx);

/// @}

/// @}
