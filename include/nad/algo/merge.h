#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/span.h"

/// writes the two sorted spans into 'dst' as one sorted run, in one linear pass.
/// 'dst' must hold a.len + b.len and must not overlap either source
NAD_API
void nad_span_merge(nad_SpanMut dst, nad_Span a, nad_Span b, nad_Cmp cmp);

/// merges the two sorted runs 's[0, mid)' and 's[mid, len)' into one, in place.
///
/// With an allocator this is a single linear pass: the shorter of the two runs is copied
/// aside — never more than half the span — and the two are merged back over the original.
/// With 'al' as nullptr, or when the allocation is refused, it falls back to a
/// buffer-free path that needs no memory at all and pays O(n log n) for it.
///
/// Not getting the buffer is therefore not an error but a slower way to the same answer,
/// which is why nothing is returned. Pass nullptr deliberately when the span is small or
/// the memory matters more than the time.
///
/// Both paths are stable, as is nad_span_merge — equal elems keep the run they came from
/// and the left run comes first — so sorting a partly sorted span means finding the
/// boundary with nad_span_is_sorted_until and calling this
NAD_API
void nad_span_inplace_merge(nad_SpanMut s, size_t mid, nad_Cmp cmp, nad_Al *al);
