#pragma once

#include "trs/algo/fn.h"
#include "trs/alloc/alloc.h"
#include "trs/core/cmp.h"
#include "trs/core/export.h"
#include "trs/core/rng.h"
#include "trs/core/span.h"
#include "trs/core/status.h"

#include <stddef.h>

/// @file

/// @defgroup algo_permute algo/permute
/// @ingroup algo
/// @brief rearranging a span without changing what it holds
///
/// All in place and allocating nothing — except the one that keeps an order, which says
/// so by taking an allocator and returning a trs_Status. The two that draw an order
/// rather than compute it say so by taking a trs_Rng.
///
/// @par Example
/// @snippet algo/example_permute.c pred
/// @snippet algo/example_permute.c move
/// @snippet algo/example_permute.c shuffle
/// @snippet algo/example_permute.c partition
/// @{

/// @name move
/// @{

/// turns the span back to front
/// @param s the span
/// @bigo{n}
TRS_API
void trs_span_reverse(trs_SpanMut s);

/// rotates left so that the elem at 'mid' becomes the first
/// @param s the span
/// @param mid the new front; asserts mid <= s.len, and 0 or s.len is a no-op
/// @bigo{n} — three reversals, no buffer
TRS_API
void trs_span_rotate(trs_SpanMut s, size_t mid);

/// exchanges the elems of two spans, position by position
/// @param a one span
/// @param b the other; asserts the same len and elem_size
/// @bigo{n}
TRS_API
void trs_span_swap_ranges(trs_SpanMut a, trs_SpanMut b);

/// @}

/// @name permutations
/// @{

/// rearranges into the next permutation in 'cmp' order
/// @param s the span
/// @param cmp the order permutations are counted in
/// @return whether there was one; on the last it wraps to the first and returns false,
///         so a do-while over a sorted span walks them all exactly once
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_span_next_permutation(trs_SpanMut s, trs_Cmp cmp);

/// rearranges into the previous permutation in 'cmp' order
/// @param s the span
/// @param cmp the order permutations are counted in
/// @return whether there was a previous one; on the first it wraps to the last
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_span_prev_permutation(trs_SpanMut s, trs_Cmp cmp);

/// @}

/// @name shuffle
/// @{

/// rearranges into an order drawn from 'rng'
/// @param s the span
/// @param rng where the order comes from; the same seed replays the same shuffle
/// @bigo{n} — Fisher-Yates: each position draws from the elems not placed yet, which is
///            what makes all n! orders equally likely
TRS_API
void trs_span_shuffle(trs_SpanMut s, trs_Rng *rng);

/// shuffles only far enough to settle the first 'count' positions
/// @param s the span; every elem may move, not just the first 'count'
/// @param count how many positions to settle; asserts count <= s.len
/// @param rng where the order comes from
/// @bigo{count} — the same walk stopped early, so the prefix is a uniform sample of the
///                whole span and the rest holds what is left, in no promised order
TRS_API
void trs_span_shuffle_prefix(trs_SpanMut s, size_t count, trs_Rng *rng);

/// @}

/// @name partition
/// @{

/// moves the elems satisfying 'pred' to the front
/// @param s the span
/// @param pred the test
/// @param ctx handed to 'pred'
/// @return where they end — the index of the first elem that does not satisfy 'pred'
/// @bigo{n} — it swaps, so neither side keeps the order it had
[[nodiscard]] TRS_API
size_t trs_span_partition(trs_SpanMut s, trs_Pred pred, void *ctx);

/// the same split, both sides keeping the order they had
/// @param s the span
/// @param pred the test, asked about each elem once
/// @param ctx handed to 'pred'
/// @param al where the scratch comes from; an order costs one span-sized, as in
///           trs_span_sort_stable
/// @param[out] out_boundary where the front part ends, written only on success
/// @retval TRS_STATUS_OK on success
/// @retval TRS_STATUS_ERR_NO_MEM when 'al' cannot give the buffer; 's' is untouched
/// @bigo{n}
[[nodiscard]] TRS_API
trs_Status trs_span_partition_stable(
    trs_SpanMut s,
    trs_Pred pred,
    void *ctx,
    trs_Al *al,
    size_t *out_boundary
);

/// whether the span is already split that way
/// @param s the span
/// @param pred the test
/// @param ctx handed to 'pred'
/// @return whether every elem satisfying 'pred' comes before every elem that does not
/// @bigo{n}
[[nodiscard]] TRS_API
bool trs_span_is_partitioned(trs_Span s, trs_Pred pred, void *ctx);

/// @}

/// @}
