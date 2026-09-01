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

[[nodiscard]]
static size_t node_bytes(size_t elem_size);

[[nodiscard]]
static nad_Status node_new(nad_Al *al, size_t elem_size, const void *val, nad_ListNode **out);

static void node_drop(nad_Al *al, size_t elem_size, nad_ListNode *node);

static void link_node(nad_List *self, nad_ListNode *node, nad_ListNode *prev, nad_ListNode *next);

[[nodiscard]]
static nad_Status insert_between(nad_List *self, nad_ListNode *prev, nad_ListNode *next, const void *val);

static void unlink_node(nad_List *self, nad_ListNode *node);

static void remove_node(nad_List *self, nad_ListNode *node);

static void splice_nodes(nad_List *self, nad_List *src, bool front);

static void swap_contents(nad_List *a, nad_List *b);

static void clear_nodes(nad_List *self);

[[nodiscard]]
static nad_Status clone_into(const nad_List *self, nad_Al *al, nad_List **out);

[[nodiscard]] [[maybe_unused]]
static bool owns_node(const nad_List *self, const nad_ListNode *node);

/// merges two chains linked through 'next' alone and returns the head of the result.
/// Equal elems keep 'a' before 'b', which is what makes the sort stable. 'prev' is left
/// wrong on purpose: relink_prev repairs it once, at the end, instead of on every step
[[nodiscard]]
static nad_ListNode *merge_chains(nad_ListNode *a, nad_ListNode *b, nad_Cmp cmp);

/// sorts a chain of 'len' nodes linked through 'next' alone and returns its new head
[[nodiscard]]
static nad_ListNode *sort_chain(nad_ListNode *head, size_t len, nad_Cmp cmp);

/// walks the list forward and rebuilds every 'prev' and the tail from the 'next' chain
static void relink_prev(nad_List *self);

/// merges 'src' into 'self' by relinking and leaves 'src' empty; both already sorted
static void merge_into(nad_List *self, nad_List *src, nad_Cmp cmp);

/* ========== lifetime ========== */

nad_Status nad_list_new(size_t elem_size, nad_Al *al, nad_List **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_List *obj = nad_alloc(al, sizeof(nad_List));
    if (!obj) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    obj->head = nullptr;
    obj->tail = nullptr;
    obj->len = 0;
    obj->elem_size = elem_size;
    obj->al = al;

    ASSERT_LIST(obj);

    *out = obj;

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

/* ========== compare ========== */

bool nad_list_eq(const nad_List *a, const nad_List *b) {
    ASSERT_LIST(a);
    ASSERT_LIST(b);
    assert(a->elem_size == b->elem_size);

    if (a == b) {
        return true;
    }

    if (a->len != b->len) {
        return false;
    }

    const nad_ListNode *x = a->head;
    const nad_ListNode *y = b->head;
    while (x) {
        if (memcmp(x->elem, y->elem, a->elem_size) != 0) {
            return false;
        }
        x = x->next;
        y = y->next;
    }

    return true;
}

bool nad_list_eq_by(const nad_List *a, const nad_List *b, nad_Eq eq) {
    ASSERT_LIST(a);
    ASSERT_LIST(b);
    assert(a->elem_size == b->elem_size);
    assert(eq);

    if (a == b) {
        return true;
    }

    if (a->len != b->len) {
        return false;
    }

    const nad_ListNode *x = a->head;
    const nad_ListNode *y = b->head;
    while (x) {
        if (!eq(x->elem, y->elem)) {
            return false;
        }
        x = x->next;
        y = y->next;
    }

    return true;
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

    return insert_between(self, nullptr, self->head, val);
}

nad_Status nad_list_push_back(nad_List *self, const void *val) {
    ASSERT_LIST(self);
    assert(val);

    return insert_between(self, self->tail, nullptr, val);
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

    return insert_between(self, at->prev, at, val);
}

nad_Status nad_list_insert_after(nad_List *self, nad_ListNode *at, const void *val) {
    ASSERT_LIST(self);
    ASSERT_NODE(at);
    assert(owns_node(self, at));
    assert(val);

    return insert_between(self, at, at->next, val);
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

nad_Status nad_list_splice_node(nad_List *self, nad_ListNode *at, nad_List *src, nad_ListNode *node) {
    ASSERT_LIST(self);
    ASSERT_LIST(src);
    assert(self->elem_size == src->elem_size);
    assert(node);
    assert(owns_node(src, node));
    assert(!at || owns_node(self, at));
    assert(at != node);

    if (self->al != src->al) {
        // a node belongs to the allocator that made it, so it cannot change lists: the
        // elem is copied into a node of 'self' and the old one goes
        nad_ListNode *prev = at ? at->prev : self->tail;
        const nad_Status st = insert_between(self, prev, at, node->elem);
        if (NAD_STATUS_IS_ERR(st)) {
            return st;
        }

        remove_node(src, node);

        ASSERT_LIST(self);
        ASSERT_LIST(src);

        return NAD_STATUS_OK;
    }

    // unlinking first is what makes 'self == src' work: 'at->prev' is read from a list
    // that no longer holds 'node', so moving a node one step forward lands where it must
    unlink_node(src, node);
    link_node(self, node, at ? at->prev : self->tail, at);

    ASSERT_LIST(self);
    ASSERT_LIST(src);
    ASSERT_NODE(node);

    return NAD_STATUS_OK;
}

/* ========== relink ========== */

void nad_list_reverse(nad_List *self) {
    ASSERT_LIST(self);

    nad_ListNode *cur = self->head;
    while (cur) {
        nad_ListNode *next = cur->next;
        NAD_SWAP(cur->next, cur->prev);
        cur = next;
    }

    NAD_SWAP(self->head, self->tail);

    ASSERT_LIST(self);
}

void nad_list_sort(nad_List *self, nad_Cmp cmp) {
    ASSERT_LIST(self);
    assert(cmp);

    if (self->len < 2) {
        return;
    }

    self->head = sort_chain(self->head, self->len, cmp);
    relink_prev(self);

    ASSERT_LIST(self);
}

nad_Status nad_list_merge(nad_List *self, nad_List *src, nad_Cmp cmp) {
    ASSERT_LIST(self);
    ASSERT_LIST(src);
    assert(self != src);
    assert(self->elem_size == src->elem_size);
    assert(cmp);

    if (src->len == 0) {
        return NAD_STATUS_OK;
    }

    if (self->al == src->al) {
        merge_into(self, src, cmp);
        return NAD_STATUS_OK;
    }

    nad_List *copy;
    const nad_Status st = clone_into(src, self->al, &copy);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    merge_into(self, copy, cmp);
    nad_list_drop(copy);
    clear_nodes(src);

    ASSERT_LIST(self);
    ASSERT_LIST(src);

    return NAD_STATUS_OK;
}

/* ========== copy to span ========== */

void nad_list_copy_to_span(const nad_List *self, nad_SpanMut dst) {
    ASSERT_LIST(self);
    NAD_SPAN_ASSERT(dst);
    assert(dst.elem_size == self->elem_size);
    assert(dst.len == self->len);

    size_t i = 0;
    for (const nad_ListNode *node = self->head; node; node = node->next, ++i) {
        memcpy(nad_byte_offset_mut(dst.data, self->elem_size, i), node->elem, self->elem_size);
    }
}

void nad_list_copy_from_span(nad_List *self, nad_Span src) {
    ASSERT_LIST(self);
    NAD_SPAN_ASSERT(src);
    assert(src.elem_size == self->elem_size);
    assert(src.len == self->len);

    size_t i = 0;
    for (nad_ListNode *node = self->head; node; node = node->next, ++i) {
        memcpy(node->elem, nad_byte_offset(src.data, self->elem_size, i), self->elem_size);
    }
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

static size_t node_bytes(size_t elem_size) {
    return sizeof(nad_ListNode) + elem_size;
}

static nad_Status node_new(nad_Al *al, size_t elem_size, const void *val, nad_ListNode **out) {
    assert(al);
    assert(elem_size > 0);
    assert(val);
    assert(out);

    size_t bytes;
    if (ckd_add(&bytes, sizeof(nad_ListNode), elem_size)) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    nad_ListNode *node = nad_alloc(al, bytes);
    if (!node) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    assert(nad_ptr_is_aligned(node, alignof(max_align_t)));

    node->next = nullptr;
    node->prev = nullptr;
    memcpy(node->elem, val, elem_size);

    *out = node;

    return NAD_STATUS_OK;
}

static void node_drop(nad_Al *al, size_t elem_size, nad_ListNode *node) {
    nad_dealloc(al, node, node_bytes(elem_size));
}

static void link_node(nad_List *self, nad_ListNode *node, nad_ListNode *prev, nad_ListNode *next) {
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

// the whole of push_front/push_back/insert_before/insert_after: the four differ only in
// which pair of neighbours they hand over, and link_node already reads a null neighbour
// as "this end of the list"
static nad_Status insert_between(nad_List *self, nad_ListNode *prev, nad_ListNode *next, const void *val) {
    nad_ListNode *node;
    const nad_Status st = node_new(self->al, self->elem_size, val, &node);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    link_node(self, node, prev, next);

    ASSERT_LIST(self);
    ASSERT_NODE(node);

    return NAD_STATUS_OK;
}

static void unlink_node(nad_List *self, nad_ListNode *node) {
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

static void remove_node(nad_List *self, nad_ListNode *node) {
    unlink_node(self, node);
    node_drop(self->al, self->elem_size, node);

    ASSERT_LIST(self);
}

static void splice_nodes(nad_List *self, nad_List *src, bool front) {
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

static void swap_contents(nad_List *a, nad_List *b) {
    NAD_SWAP(a->head, b->head);
    NAD_SWAP(a->tail, b->tail);
    NAD_SWAP(a->len, b->len);
}

static void clear_nodes(nad_List *self) {
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

static nad_Status clone_into(const nad_List *self, nad_Al *al, nad_List **out) {
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

static nad_ListNode *merge_chains(nad_ListNode *a, nad_ListNode *b, nad_Cmp cmp) {
    assert(cmp);

    nad_ListNode *head = nullptr;
    nad_ListNode **tail = &head;

    while (a && b) {
        if (cmp(a->elem, b->elem) <= 0) {
            *tail = a;
            a = a->next;
        } else {
            *tail = b;
            b = b->next;
        }
        tail = &(*tail)->next;
    }

    *tail = a ? a : b;

    return head;
}

static nad_ListNode *sort_chain(nad_ListNode *head, size_t len, nad_Cmp cmp) {
    assert(head);
    assert(cmp);

    if (len < 2) {
        return head;
    }

    const size_t half = len / 2;

    // walk to the LAST node of the left half, so the chain can be cut behind it
    nad_ListNode *left_tail = head;
    for (size_t i = 1; i < half; ++i) {
        left_tail = left_tail->next;
    }

    nad_ListNode *right = left_tail->next;
    left_tail->next = nullptr;

    return merge_chains(sort_chain(head, half, cmp), sort_chain(right, len - half, cmp), cmp);
}

static void relink_prev(nad_List *self) {
    nad_ListNode *prev = nullptr;

    for (nad_ListNode *node = self->head; node; node = node->next) {
        node->prev = prev;
        prev = node;
    }

    self->tail = prev;
}

static void merge_into(nad_List *self, nad_List *src, nad_Cmp cmp) {
    assert(self->al == src->al);
    assert(self->elem_size == src->elem_size);
    assert(src->len > 0);

    self->head = merge_chains(self->head, src->head, cmp);
    self->len += src->len;
    relink_prev(self);

    src->head = nullptr;
    src->tail = nullptr;
    src->len = 0;

    ASSERT_LIST(self);
    ASSERT_LIST(src);
}

static bool owns_node(const nad_List *self, const nad_ListNode *node) {
    for (const nad_ListNode *cur = self->head; cur; cur = cur->next) {
        if (cur == node) {
            return true;
        }
    }
    return false;
}
