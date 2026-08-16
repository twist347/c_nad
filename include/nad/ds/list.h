#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/cmp.h"
#include "nad/core/export.h"
#include "nad/core/print.h"
#include "nad/core/status.h"
#include "nad/ds/span.h"

#include <stddef.h>

/// doubly linked owning list of type-erased elems
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

/* ========== print ========== */

NAD_API
void nad_list_fprint(const nad_List *self, FILE *stream, nad_FPrint fprint);

NAD_API
void nad_list_print(const nad_List *self, nad_FPrint fprint);
