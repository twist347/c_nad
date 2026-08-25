#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/core/status.h"

#include <stddef.h>

/// doubly linked owning list of type-erased elems: every elem sits in a node of its own,
/// and the nodes are held together by pointers rather than by being neighbours in memory.
///
/// What that buys is the guarantee no contiguous container can make: a node never moves,
/// so a position stays good for as long as the elem does. What it costs is everything
/// contiguity gives — an allocation per elem, memory scattered across the heap, no index,
/// and reaching the n-th elem is a walk rather than a multiplication.
///
/// Insertion and removal are O(1) anywhere a node is already in hand, which is the other
/// half of the trade: a vec pays O(n) to open or close a gap, a list pays nothing.
///
/// The relink family — reverse, sort, merge and splice_node — does its work by moving
/// NODES rather than elems, so a borrowed node still names the elem it named before.
/// Sorting a copy would leave every such pointer naming something else, which is why a
/// list sorts itself instead of handing the work to algo/sort.
///
/// The bridge to algo is the pair nad_list_copy_to_span / nad_list_copy_from_span: the
/// elems are not contiguous, so there is no view to hand over and nothing cheaper than a
/// copy. Splice and swap want both lists on one allocator — a node cannot change owner
/// without moving, and moving is what this type exists not to do.
typedef struct nad_List nad_List;

/// a position in the list; borrowed from the list
/// and invalidated only by removing that very elem
typedef struct nad_ListNode nad_ListNode;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Status nad_list_new(size_t elem_size, nad_Al *al, nad_List **out);

[[nodiscard]] NAD_API
nad_Status nad_list_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_List **out);

[[nodiscard]] NAD_API
nad_Status nad_list_from_span(nad_Span s, nad_Al *al, nad_List **out);

NAD_API
void nad_list_drop(nad_List *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API
nad_Status nad_list_copy(const nad_List *self, nad_List **out);

[[nodiscard]] NAD_API
nad_Status nad_list_copy_assign(const nad_List *self, nad_List *other);

/* ========== compare ========== */

/// whether the two hold the same elems, byte for byte. Same elem_size for both —
/// a mismatch there is a programmer error, not a false; differing lengths are just false
[[nodiscard]] NAD_API
bool nad_list_eq(const nad_List *a, const nad_List *b);

/// whether the two hold equal elems under 'eq', which is asked of every pair until one
/// says no
[[nodiscard]] NAD_API
bool nad_list_eq_by(const nad_List *a, const nad_List *b, nad_Eq eq);

/* ========== info ========== */

[[nodiscard]] NAD_API
size_t nad_list_len(const nad_List *self);

[[nodiscard]] NAD_API
size_t nad_list_elem_size(const nad_List *self);

[[nodiscard]] NAD_API
nad_Al *nad_list_al(const nad_List *self);

/* ========== access ========== */

[[nodiscard]] NAD_API
const void *nad_list_first(const nad_List *self);

[[nodiscard]] NAD_API
void *nad_list_first_mut(nad_List *self);

[[nodiscard]] NAD_API
const void *nad_list_last(const nad_List *self);

[[nodiscard]] NAD_API
void *nad_list_last_mut(nad_List *self);

/* ========== nodes ========== */

[[nodiscard]] NAD_API
const nad_ListNode *nad_list_first_node(const nad_List *self);

[[nodiscard]] NAD_API
nad_ListNode *nad_list_first_node_mut(nad_List *self);

[[nodiscard]] NAD_API
const nad_ListNode *nad_list_last_node(const nad_List *self);

[[nodiscard]] NAD_API
nad_ListNode *nad_list_last_node_mut(nad_List *self);

[[nodiscard]] NAD_API
const nad_ListNode *nad_list_node_next(const nad_ListNode *node);

[[nodiscard]] NAD_API
nad_ListNode *nad_list_node_next_mut(nad_ListNode *node);

[[nodiscard]] NAD_API
const nad_ListNode *nad_list_node_prev(const nad_ListNode *node);

[[nodiscard]] NAD_API
nad_ListNode *nad_list_node_prev_mut(nad_ListNode *node);

[[nodiscard]] NAD_API
const void *nad_list_node_elem(const nad_ListNode *node);

[[nodiscard]] NAD_API
void *nad_list_node_elem_mut(nad_ListNode *node);

/* ========== mods ========== */

[[nodiscard]] NAD_API
nad_Status nad_list_push_front(nad_List *self, const void *val);

[[nodiscard]] NAD_API
nad_Status nad_list_push_back(nad_List *self, const void *val);

NAD_API
void nad_list_pop_front(nad_List *self);

NAD_API
void nad_list_pop_back(nad_List *self);

[[nodiscard]] NAD_API
nad_Status nad_list_insert_before(nad_List *self, nad_ListNode *at, const void *val);

[[nodiscard]] NAD_API
nad_Status nad_list_insert_after(nad_List *self, nad_ListNode *at, const void *val);

NAD_API
void nad_list_remove(nad_List *self, nad_ListNode *node);

NAD_API
void nad_list_clear(nad_List *self);

/// moves every node of 'src' to the front of 'self', leaving 'src' empty;
/// O(1) if both share al, a copy of every elem otherwise
[[nodiscard]] NAD_API
nad_Status nad_list_splice_front(nad_List *self, nad_List *src);

/// moves every node of 'src' to the back of 'self', leaving 'src' empty;
/// O(1) if both share al, a copy of every elem otherwise
[[nodiscard]] NAD_API
nad_Status nad_list_splice_back(nad_List *self, nad_List *src);

/// both lists must share an allocator: the nodes change list
/// without moving, so every borrowed node stays valid
NAD_API
void nad_list_swap(nad_List *self, nad_List *other);

/// moves ONE node out of 'src' and into 'self' before 'at'; 'at == nullptr' means the
/// back. 'src' and 'self' may be the same list, which moves the node within it.
///
/// O(1) when both share an allocator, and the node keeps its address, so a borrowed
/// pointer to it survives the move. On two allocators the elem is copied into a node of
/// 'self' and the old one goes, exactly as in splice_front — the address does not survive
[[nodiscard]] NAD_API
nad_Status nad_list_splice_node(nad_List *self, nad_ListNode *at, nad_List *src, nad_ListNode *node);

/* ========== relink ========== */

/// reverses the list by relinking, O(n) and no allocation. Every borrowed node keeps its
/// address and its elem — only the order changes
NAD_API
void nad_list_reverse(nad_List *self);

/// sorts by relinking: stable, O(n log n), no allocation.
///
/// This is not the same operation as sorting a copy. The elems do not move between
/// nodes — the nodes themselves change places — so a borrowed node still names the elem
/// it named before. Sorting a copy would leave every such pointer naming something else,
/// which is why a list sorts itself instead of handing the work to algo/sort
NAD_API
void nad_list_sort(nad_List *self, nad_Cmp cmp);

/// merges the sorted 'src' into the sorted 'self' and leaves 'src' empty, keeping equal
/// elems of 'self' before those of 'src'. Both must already be sorted under 'cmp'.
///
/// O(n + m) and no allocation when both share an allocator, a copy of every elem
/// otherwise — the same two paths splice_back has. Reaching the same result through
/// splice_back plus sort would cost O(n log n) and throw away what is already known
[[nodiscard]] NAD_API
nad_Status nad_list_merge(nad_List *self, nad_List *src, nad_Cmp cmp);

/* ========== copy to span ========== */

/// writes every elem into 'dst', front to back. 'dst' must be exactly as long as the
/// list, the same rule nad_span_copy holds callers to.
///
/// This is the list's only bridge to algo: the elems are not contiguous, so there is no
/// view to hand over and nothing cheaper than a copy. Search it, count it, fold it
NAD_API
void nad_list_copy_to_span(const nad_List *self, nad_SpanMut dst);

/// overwrites every elem from 'src', which must be exactly as long as the list. The pair
/// to nad_list_copy_to_span: take the contents out, hand them to algo, put the answer
/// back. The nodes stay where they are, so nothing is allocated and nothing can fail
NAD_API
void nad_list_copy_from_span(nad_List *self, nad_Span src);

/* ========== print ========== */

NAD_API
void nad_list_fprint(const nad_List *self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_list_print(const nad_List *self, nad_FPrint fprint);

/* ========== macros ========== */

#define NAD_LIST_NEW(T, al, out) \
    nad_list_new(sizeof(T), (al), (out))

#define NAD_LIST_FROM_DATA(T, data, len, al, out) \
    nad_list_from_data((const T *){ (data) }, (len), sizeof(T), (al), (out))

#define NAD_LIST_OF(T, al, out, ...)                    \
    nad_list_from_data(                                 \
        (const T[]){ __VA_ARGS__ },                     \
        sizeof((const T[]){ __VA_ARGS__ }) / sizeof(T), \
        sizeof(T), (al), (out))

#define NAD_LIST_FIRST_AS(T, self) \
    ((const T *) nad_list_first((self)))

#define NAD_LIST_FIRST_MUT_AS(T, self) \
    ((T *) nad_list_first_mut((self)))

#define NAD_LIST_LAST_AS(T, self) \
    ((const T *) nad_list_last((self)))

#define NAD_LIST_LAST_MUT_AS(T, self) \
    ((T *) nad_list_last_mut((self)))

#define NAD_LIST_NODE_ELEM_AS(T, node) \
    ((const T *) nad_list_node_elem((node)))

#define NAD_LIST_NODE_ELEM_MUT_AS(T, node) \
    ((T *) nad_list_node_elem_mut((node)))

#define NAD_LIST_PUSH_FRONT(T, self, val) \
    nad_list_push_front((self), &(T){ (val) })

#define NAD_LIST_PUSH_BACK(T, self, val) \
    nad_list_push_back((self), &(T){ (val) })

#define NAD_LIST_INSERT_BEFORE(T, self, at, val) \
    nad_list_insert_before((self), (at), &(T){ (val) })

#define NAD_LIST_INSERT_AFTER(T, self, at, val) \
    nad_list_insert_after((self), (at), &(T){ (val) })
