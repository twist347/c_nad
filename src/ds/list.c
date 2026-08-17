#include "nad/ds/list.h"

#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

#include "internal/todo.h"

/* ========== internals ========== */

#define ASSERT_LIST(l)                                          \
    (assert(l),                                                 \
     assert((l)->elem_size > 0),                                \
     assert((l)->al),                                           \
     assert(((l)->len > 0) == ((l)->head != nullptr)),          \
     assert(((l)->head != nullptr) == ((l)->tail != nullptr)))

#define ASSERT_NODE(n)                             \
    (assert(n),                                    \
     assert(!(n)->prev || (n)->prev->next == (n)), \
     assert(!(n)->next || (n)->next->prev == (n)))

struct nad_ListNode {
    nad_ListNode *next;
    nad_ListNode *prev;
    alignas(max_align_t) char elem[];
};

struct nad_List {
    nad_ListNode *head;
    nad_ListNode *tail;
    size_t len;
    size_t elem_size;
    nad_Al *al;
};

[[nodiscard]] static
size_t node_bytes(size_t elem_size);

[[nodiscard]] static
nad_Status node_new(nad_Al *al, size_t elem_size, const void *val, nad_ListNode **out);

static
void node_drop(nad_Al *al, size_t elem_size, nad_ListNode *node);

/* ========== lifetime ========== */

nad_Status nad_list_new(size_t elem_size, nad_Al *al, nad_List **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_List *list = nad_alloc(al, sizeof(nad_List));
    if (!list) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    list->head = nullptr;
    list->tail = nullptr;
    list->len = 0;
    list->elem_size = elem_size;
    list->al = al;

    ASSERT_LIST(list);

    *out = list;

    return NAD_STATUS_OK;
}

nad_Status nad_list_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_List **out) {
    NAD_NOT_IMPLEMENTED(data, len, elem_size, al, out);
}

nad_Status nad_list_from_span(nad_Span s, nad_Al *al, nad_List **out) {
    NAD_NOT_IMPLEMENTED(s, al, out);
}

void nad_list_drop(nad_List *self) {
    NAD_NOT_IMPLEMENTED(self);
}

/* ========== copy ========== */

nad_Status nad_list_copy(const nad_List *self, nad_List **out) {
    NAD_NOT_IMPLEMENTED(self, out);
}

nad_Status nad_list_copy_assign(const nad_List *self, nad_List *other) {
    NAD_NOT_IMPLEMENTED(self, other);
}

/* ========== info ========== */

size_t nad_list_len(const nad_List *self) {
    ASSERT_LIST(self);

    return self->len;
}

size_t nad_list_elem_size(const nad_List *self) {
    ASSERT_LIST(self);

    return self->elem_size;
}

nad_Al *nad_list_al(const nad_List *self) {
    ASSERT_LIST(self);

    return self->al;
}

/* ========== access ========== */

const void *nad_list_first(const nad_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    return self->head->elem;
}

void *nad_list_first_mut(nad_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    return self->head->elem;
}

const void *nad_list_last(const nad_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    return self->tail->elem;
}

void *nad_list_last_mut(nad_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    return self->tail->elem;
}

/* ========== nodes ========== */

const nad_ListNode *nad_list_first_node(const nad_List *self) {
    ASSERT_LIST(self);

    return self->head;
}

nad_ListNode *nad_list_first_node_mut(nad_List *self) {
    ASSERT_LIST(self);

    return self->head;
}

const nad_ListNode *nad_list_last_node(const nad_List *self) {
    ASSERT_LIST(self);

    return self->tail;
}

nad_ListNode *nad_list_last_node_mut(nad_List *self) {
    ASSERT_LIST(self);

    return self->tail;
}

const nad_ListNode *nad_list_node_next(const nad_ListNode *node) {
    ASSERT_NODE(node);

    return node->next;
}

nad_ListNode *nad_list_node_next_mut(nad_ListNode *node) {
    ASSERT_NODE(node);

    return node->next;
}

const nad_ListNode *nad_list_node_prev(const nad_ListNode *node) {
    ASSERT_NODE(node);

    return node->prev;
}

nad_ListNode *nad_list_node_prev_mut(nad_ListNode *node) {
    ASSERT_NODE(node);

    return node->prev;
}

const void *nad_list_node_elem(const nad_ListNode *node) {
    ASSERT_NODE(node);

    return node->elem;
}

void *nad_list_node_elem_mut(nad_ListNode *node) {
    ASSERT_NODE(node);

    return node->elem;
}

/* ========== mods ========== */

nad_Status nad_list_push_front(nad_List *self, const void *val) {
    NAD_NOT_IMPLEMENTED(self, val);
}

nad_Status nad_list_push_back(nad_List *self, const void *val) {
    NAD_NOT_IMPLEMENTED(self, val);
}

void nad_list_pop_front(nad_List *self) {
    NAD_NOT_IMPLEMENTED(self);
}

void nad_list_pop_back(nad_List *self) {
    NAD_NOT_IMPLEMENTED(self);
}

/* ========== print ========== */

void nad_list_fprint(const nad_List *self, FILE *stream, nad_FPrint fprint) {
    ASSERT_LIST(self);
    assert(stream);
    assert(fprint);

    fputc('[', stream);
    for (const nad_ListNode *node = self->head; node; node = node->next) {
        if (node != self->head) {
            fputs(", ", stream);
        }
        fprint(stream, node->elem);
    }
    fputs("]\n", stream);
}

void nad_list_print(const nad_List *self, nad_FPrint fprint) {
    ASSERT_LIST(self);
    assert(fprint);

    nad_list_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

static
size_t node_bytes(size_t elem_size) {
    return sizeof(nad_ListNode) + elem_size;
}

static
nad_Status node_new(nad_Al *al, size_t elem_size, const void *val, nad_ListNode **out) {
    assert(al);
    assert(elem_size > 0);
    assert(val);
    assert(out);

    size_t bytes;
    if (ckd_add(&bytes, sizeof(nad_ListNode), elem_size)) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    nad_ListNode *node = nad_alloc(al, bytes);
    if (!node) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    assert(nad_ptr_is_aligned(node, alignof(max_align_t)));

    node->next = nullptr;
    node->prev = nullptr;
    memcpy(node->elem, val, elem_size);

    *out = node;

    return NAD_STATUS_OK;
}

static
void node_drop(nad_Al *al, size_t elem_size, nad_ListNode *node) {
    nad_dealloc(al, node, node_bytes(elem_size));
}
