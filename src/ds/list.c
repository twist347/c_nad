#include "nad/ds/list.h"

#include "nad/core/util.h"
#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

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
    alignas(max_align_t) unsigned char elem[];
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

static
void link_node(nad_List *self, nad_ListNode *node, nad_ListNode *prev, nad_ListNode *next);

static
void unlink_node(nad_List *self, nad_ListNode *node);

static
void remove_node(nad_List *self, nad_ListNode *node);

static
void splice_nodes(nad_List *self, nad_List *src, bool front);

static
void swap_contents(nad_List *a, nad_List *b);

static
void clear_nodes(nad_List *self);

[[nodiscard]] static
nad_Status clone_into(const nad_List *self, nad_Al *al, nad_List **out);

[[nodiscard]] [[maybe_unused]] static
bool owns_node(const nad_List *self, const nad_ListNode *node);

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
    assert(data || len == 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_List *list;
    nad_Status st = nad_list_new(elem_size, al, &list);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    for (size_t i = 0; i < len; ++i) {
        st = nad_list_push_back(list, nad_byte_offset(data, elem_size, i));
        if (NAD_STATUS_IS_ERR(st)) {
            nad_list_drop(list);
            return st;
        }
    }

    *out = list;

    return NAD_STATUS_OK;
}

nad_Status nad_list_from_span(nad_Span s, nad_Al *al, nad_List **out) {
    NAD_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return nad_list_from_data(s.data, s.len, s.elem_size, al, out);
}

void nad_list_drop(nad_List *self) {
    if (!self) {
        return;
    }

    ASSERT_LIST(self);

    nad_Al *al_copy = self->al;
    clear_nodes(self);
    nad_dealloc(al_copy, self, sizeof(nad_List));
}

/* ========== copy ========== */

nad_Status nad_list_copy(const nad_List *self, nad_List **out) {
    ASSERT_LIST(self);
    assert(out);

    return clone_into(self, self->al, out);
}

nad_Status nad_list_copy_assign(const nad_List *self, nad_List *other) {
    ASSERT_LIST(self);
    ASSERT_LIST(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    const nad_ListNode *src = self->head;
    const nad_ListNode *dst = other->head;

    while (src && dst) {
        src = src->next;
        dst = dst->next;
    }

    // 'src' is the first elem the target has no node for; those nodes are
    // allocated up front, so the only failure happens before any mutation
    nad_List spare = {
        .head = nullptr,
        .tail = nullptr,
        .len = 0,
        .elem_size = other->elem_size,
        .al = other->al,
    };

    for (const nad_ListNode *node = src; node; node = node->next) {
        const nad_Status st = nad_list_push_back(&spare, node->elem);
        if (NAD_STATUS_IS_ERR(st)) {
            clear_nodes(&spare);
            return st;
        }
    }

    // from here on nothing can fail
    const nad_ListNode *from = self->head;
    for (nad_ListNode *to = other->head; to && from; to = to->next, from = from->next) {
        memcpy(to->elem, from->elem, other->elem_size);
    }

    while (other->len > self->len) {
        remove_node(other, other->tail);
    }

    if (spare.len > 0) {
        splice_nodes(other, &spare, false);
    }

    ASSERT_LIST(other);

    return NAD_STATUS_OK;
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
    ASSERT_LIST(self);
    assert(val);

    nad_ListNode *node;
    const nad_Status st = node_new(self->al, self->elem_size, val, &node);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    link_node(self, node, nullptr, self->head);

    ASSERT_LIST(self);
    ASSERT_NODE(node);

    return NAD_STATUS_OK;
}

nad_Status nad_list_push_back(nad_List *self, const void *val) {
    ASSERT_LIST(self);
    assert(val);

    nad_ListNode *node;
    const nad_Status st = node_new(self->al, self->elem_size, val, &node);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    link_node(self, node, self->tail, nullptr);

    ASSERT_LIST(self);
    ASSERT_NODE(node);

    return NAD_STATUS_OK;
}

void nad_list_pop_front(nad_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    remove_node(self, self->head);
}

void nad_list_pop_back(nad_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    remove_node(self, self->tail);
}

nad_Status nad_list_insert_before(nad_List *self, nad_ListNode *at, const void *val) {
    ASSERT_LIST(self);
    ASSERT_NODE(at);
    assert(owns_node(self, at));
    assert(val);

    nad_ListNode *node;
    const nad_Status st = node_new(self->al, self->elem_size, val, &node);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    link_node(self, node, at->prev, at);

    ASSERT_LIST(self);
    ASSERT_NODE(node);

    return NAD_STATUS_OK;
}

nad_Status nad_list_insert_after(nad_List *self, nad_ListNode *at, const void *val) {
    ASSERT_LIST(self);
    ASSERT_NODE(at);
    assert(owns_node(self, at));
    assert(val);

    nad_ListNode *node;
    const nad_Status st = node_new(self->al, self->elem_size, val, &node);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    link_node(self, node, at, at->next);

    ASSERT_LIST(self);
    ASSERT_NODE(node);

    return NAD_STATUS_OK;
}

void nad_list_remove(nad_List *self, nad_ListNode *node) {
    ASSERT_LIST(self);
    ASSERT_NODE(node);
    assert(owns_node(self, node));

    remove_node(self, node);
}

void nad_list_clear(nad_List *self) {
    ASSERT_LIST(self);

    clear_nodes(self);

    ASSERT_LIST(self);
}

nad_Status nad_list_splice_front(nad_List *self, nad_List *src) {
    ASSERT_LIST(self);
    ASSERT_LIST(src);
    assert(self != src);
    assert(self->elem_size == src->elem_size);

    if (src->len == 0) {
        return NAD_STATUS_OK;
    }

    if (self->al == src->al) {
        splice_nodes(self, src, true);
        return NAD_STATUS_OK;
    }

    nad_List *copy;
    const nad_Status st = clone_into(src, self->al, &copy);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    splice_nodes(self, copy, true);
    nad_list_drop(copy);
    clear_nodes(src);

    ASSERT_LIST(self);
    ASSERT_LIST(src);

    return NAD_STATUS_OK;
}

nad_Status nad_list_splice_back(nad_List *self, nad_List *src) {
    ASSERT_LIST(self);
    ASSERT_LIST(src);
    assert(self != src);
    assert(self->elem_size == src->elem_size);

    if (src->len == 0) {
        return NAD_STATUS_OK;
    }

    if (self->al == src->al) {
        splice_nodes(self, src, false);
        return NAD_STATUS_OK;
    }

    nad_List *copy;
    const nad_Status st = clone_into(src, self->al, &copy);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    splice_nodes(self, copy, false);
    nad_list_drop(copy);
    clear_nodes(src);

    ASSERT_LIST(self);
    ASSERT_LIST(src);

    return NAD_STATUS_OK;
}

void nad_list_swap(nad_List *self, nad_List *other) {
    ASSERT_LIST(self);
    ASSERT_LIST(other);
    assert(self->elem_size == other->elem_size);
    assert(self->al == other->al);

    if (self == other) {
        return;
    }

    swap_contents(self, other);

    ASSERT_LIST(self);
    ASSERT_LIST(other);
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

static
void link_node(nad_List *self, nad_ListNode *node, nad_ListNode *prev, nad_ListNode *next) {
    node->prev = prev;
    node->next = next;

    if (prev) {
        prev->next = node;
    } else {
        self->head = node;
    }

    if (next) {
        next->prev = node;
    } else {
        self->tail = node;
    }

    ++self->len;
}

static
void unlink_node(nad_List *self, nad_ListNode *node) {
    ASSERT_NODE(node);
    assert(self->len > 0);

    if (node->prev) {
        node->prev->next = node->next;
    } else {
        self->head = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    } else {
        self->tail = node->prev;
    }

    --self->len;
}

static
void remove_node(nad_List *self, nad_ListNode *node) {
    unlink_node(self, node);
    node_drop(self->al, self->elem_size, node);

    ASSERT_LIST(self);
}

static
void splice_nodes(nad_List *self, nad_List *src, bool front) {
    assert(self->al == src->al);
    assert(self->elem_size == src->elem_size);
    assert(src->len > 0);

    if (self->len == 0) {
        self->head = src->head;
        self->tail = src->tail;
    } else if (front) {
        src->tail->next = self->head;
        self->head->prev = src->tail;
        self->head = src->head;
    } else {
        src->head->prev = self->tail;
        self->tail->next = src->head;
        self->tail = src->tail;
    }

    self->len += src->len;

    src->head = nullptr;
    src->tail = nullptr;
    src->len = 0;

    ASSERT_LIST(self);
    ASSERT_LIST(src);
}

static
void swap_contents(nad_List *a, nad_List *b) {
    NAD_SWAP(a->head, b->head);
    NAD_SWAP(a->tail, b->tail);
    NAD_SWAP(a->len, b->len);
}

static
void clear_nodes(nad_List *self) {
    nad_ListNode *node = self->head;
    while (node) {
        nad_ListNode *next = node->next;
        node_drop(self->al, self->elem_size, node);
        node = next;
    }

    self->head = nullptr;
    self->tail = nullptr;
    self->len = 0;
}

static
nad_Status clone_into(const nad_List *self, nad_Al *al, nad_List **out) {
    assert(al);
    assert(out);

    nad_List *list;
    nad_Status st = nad_list_new(self->elem_size, al, &list);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    for (const nad_ListNode *node = self->head; node; node = node->next) {
        st = nad_list_push_back(list, node->elem);
        if (NAD_STATUS_IS_ERR(st)) {
            nad_list_drop(list);
            return st;
        }
    }

    *out = list;

    return NAD_STATUS_OK;
}

static
bool owns_node(const nad_List *self, const nad_ListNode *node) {
    for (const nad_ListNode *cur = self->head; cur; cur = cur->next) {
        if (cur == node) {
            return true;
        }
    }
    return false;
}
