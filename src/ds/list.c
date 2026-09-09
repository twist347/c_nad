#include "terse/ds/list.h"

#include "terse/core/util.h"

#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

/* ========== internals ========== */

#define ASSERT_LIST(l)                                         \
    (assert(l),                                                \
     assert((l)->elem_size > 0),                               \
     assert((l)->al),                                          \
     assert(((l)->len > 0) == ((l)->head != nullptr)),         \
     assert(((l)->head != nullptr) == ((l)->tail != nullptr)))

#define ASSERT_NODE(n)                             \
    (assert(n),                                    \
     assert(!(n)->prev || (n)->prev->next == (n)), \
     assert(!(n)->next || (n)->next->prev == (n)))

struct trs_ListNode {
    trs_ListNode *next;
    trs_ListNode *prev;
    alignas(max_align_t) unsigned char elem[];
};

struct trs_List {
    trs_ListNode *head;
    trs_ListNode *tail;
    size_t len;
    size_t elem_size;
    trs_Al *al;
};

[[nodiscard]]
static size_t node_bytes(size_t elem_size);

[[nodiscard]]
static trs_Status node_new(trs_Al *al, size_t elem_size, const void *val, trs_ListNode **out);

static void node_drop(trs_Al *al, size_t elem_size, trs_ListNode *node);

static void link_node(trs_List *self, trs_ListNode *node, trs_ListNode *prev, trs_ListNode *next);

[[nodiscard]]
static trs_Status insert_between(trs_List *self, trs_ListNode *prev, trs_ListNode *next, const void *val);

static void unlink_node(trs_List *self, trs_ListNode *node);

static void remove_node(trs_List *self, trs_ListNode *node);

static void splice_nodes(trs_List *self, trs_List *src, bool front);

static void swap_contents(trs_List *a, trs_List *b);

/// the walk both find doors take. The node comes back mutable and the const door hands it
/// out as const: the walk is the same either way
[[nodiscard]]
static trs_ListNode *find_node(const trs_List *self, const void *key, trs_Eq eq);

static void clear_nodes(trs_List *self);

[[nodiscard]] [[maybe_unused]]
static bool owns_node(const trs_List *self, const trs_ListNode *node);

/// merges two chains linked through 'next' alone and returns the head of the result.
/// Equal elems keep 'a' before 'b', which is what makes the sort stable. 'prev' is left
/// wrong on purpose: relink_prev repairs it once, at the end, instead of on every step
[[nodiscard]]
static trs_ListNode *merge_chains(trs_ListNode *a, trs_ListNode *b, trs_Cmp cmp);

/// sorts a chain of 'len' nodes linked through 'next' alone and returns its new head
[[nodiscard]]
static trs_ListNode *sort_chain(trs_ListNode *head, size_t len, trs_Cmp cmp);

/// walks the list forward and rebuilds every 'prev' and the tail from the 'next' chain
static void relink_prev(trs_List *self);

/// merges 'src' into 'self' by relinking and leaves 'src' empty; both already sorted
static void merge_into(trs_List *self, trs_List *src, trs_Cmp cmp);

/* ========== lifetime ========== */

trs_Status trs_list_new(size_t elem_size, trs_Al *al, trs_List **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_List *obj = trs_alloc(al, sizeof(trs_List));
    if (!obj) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    obj->head = nullptr;
    obj->tail = nullptr;
    obj->len = 0;
    obj->elem_size = elem_size;
    obj->al = al;

    ASSERT_LIST(obj);

    *out = obj;

    return TRS_STATUS_OK;
}

trs_Status trs_list_from_data(const void *data, size_t len, size_t elem_size, trs_Al *al, trs_List **out) {
    assert(data || len == 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_List *list;
    trs_Status st = trs_list_new(elem_size, al, &list);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    for (size_t i = 0; i < len; ++i) {
        st = trs_list_push_back(list, trs_byte_offset(data, elem_size, i));
        if (TRS_STATUS_IS_ERR(st)) {
            trs_list_drop(list);
            return st;
        }
    }

    *out = list;

    return TRS_STATUS_OK;
}

trs_Status trs_list_from_span(trs_Span s, trs_Al *al, trs_List **out) {
    TRS_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return trs_list_from_data(s.data, s.len, s.elem_size, al, out);
}

void trs_list_drop(trs_List *self) {
    if (!self) {
        return;
    }

    ASSERT_LIST(self);

    trs_Al *al_copy = self->al;
    clear_nodes(self);
    trs_dealloc(al_copy, self, sizeof(trs_List));
}

/* ========== copy ========== */

trs_Status trs_list_copy(const trs_List *self, trs_List **out) {
    ASSERT_LIST(self);

    return trs_list_copy_with(self, self->al, out);
}

trs_Status trs_list_copy_with(const trs_List *self, trs_Al *al, trs_List **out) {
    ASSERT_LIST(self);
    assert(al);
    assert(out);

    trs_List *obj;
    trs_Status st = trs_list_new(self->elem_size, al, &obj);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    for (const trs_ListNode *node = self->head; node; node = node->next) {
        st = trs_list_push_back(obj, node->elem);
        if (TRS_STATUS_IS_ERR(st)) {
            trs_list_drop(obj);
            return st;
        }
    }

    *out = obj;

    return TRS_STATUS_OK;
}

trs_Status trs_list_copy_assign(const trs_List *self, trs_List *other) {
    ASSERT_LIST(self);
    ASSERT_LIST(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    const trs_ListNode *src = self->head;
    const trs_ListNode *dst = other->head;

    while (src && dst) {
        src = src->next;
        dst = dst->next;
    }

    // 'src' is the first elem the target has no node for; those nodes are
    // allocated up front, so the only failure happens before any mutation
    trs_List spare = {
        .head = nullptr,
        .tail = nullptr,
        .len = 0,
        .elem_size = other->elem_size,
        .al = other->al,
    };

    for (const trs_ListNode *node = src; node; node = node->next) {
        const trs_Status st = trs_list_push_back(&spare, node->elem);
        if (TRS_STATUS_IS_ERR(st)) {
            clear_nodes(&spare);
            return st;
        }
    }

    // from here on nothing can fail
    const trs_ListNode *from = self->head;
    for (trs_ListNode *to = other->head; to && from; to = to->next, from = from->next) {
        memcpy(to->elem, from->elem, other->elem_size);
    }

    while (other->len > self->len) {
        remove_node(other, other->tail);
    }

    if (spare.len > 0) {
        splice_nodes(other, &spare, false);
    }

    ASSERT_LIST(other);

    return TRS_STATUS_OK;
}

trs_Status trs_list_move_assign(trs_List *self, trs_List *other) {
    ASSERT_LIST(self);
    ASSERT_LIST(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    // one allocator: the nodes change list without moving. What 'other' held ends up in 'self' and is released
    // there, through the very allocator that made it
    if (self->al == other->al) {
        TRS_SWAP(*self, *other);
        clear_nodes(self);

        ASSERT_LIST(self);
        ASSERT_LIST(other);

        return TRS_STATUS_OK;
    }

    // two allocators: the whole copy is built on the target's before anything of it is
    // touched, so a refusal leaves both as they were
    trs_List *obj;
    const trs_Status st = trs_list_copy_with(self, other->al, &obj);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    TRS_SWAP(*other, *obj);
    trs_list_drop(obj);
    clear_nodes(self);

    ASSERT_LIST(self);
    ASSERT_LIST(other);

    return TRS_STATUS_OK;
}

/* ========== compare ========== */

bool trs_list_eq(const trs_List *a, const trs_List *b) {
    ASSERT_LIST(a);
    ASSERT_LIST(b);
    assert(a->elem_size == b->elem_size);

    if (a == b) {
        return true;
    }

    if (a->len != b->len) {
        return false;
    }

    const trs_ListNode *x = a->head;
    const trs_ListNode *y = b->head;
    while (x) {
        if (memcmp(x->elem, y->elem, a->elem_size) != 0) {
            return false;
        }
        x = x->next;
        y = y->next;
    }

    return true;
}

bool trs_list_eq_by(const trs_List *a, const trs_List *b, trs_Eq eq) {
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

    const trs_ListNode *x = a->head;
    const trs_ListNode *y = b->head;
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

size_t trs_list_len(const trs_List *self) {
    ASSERT_LIST(self);

    return self->len;
}

size_t trs_list_elem_size(const trs_List *self) {
    ASSERT_LIST(self);

    return self->elem_size;
}

trs_Al *trs_list_al(const trs_List *self) {
    ASSERT_LIST(self);

    return self->al;
}

/* ========== access ========== */

const void *trs_list_front(const trs_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    return self->head->elem;
}

void *trs_list_front_mut(trs_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    return self->head->elem;
}

const void *trs_list_back(const trs_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    return self->tail->elem;
}

void *trs_list_back_mut(trs_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    return self->tail->elem;
}

/* ========== nodes ========== */

const trs_ListNode *trs_list_front_node(const trs_List *self) {
    ASSERT_LIST(self);

    return self->head;
}

trs_ListNode *trs_list_front_node_mut(trs_List *self) {
    ASSERT_LIST(self);

    return self->head;
}

const trs_ListNode *trs_list_back_node(const trs_List *self) {
    ASSERT_LIST(self);

    return self->tail;
}

trs_ListNode *trs_list_back_node_mut(trs_List *self) {
    ASSERT_LIST(self);

    return self->tail;
}

const trs_ListNode *trs_list_node_next(const trs_ListNode *node) {
    ASSERT_NODE(node);

    return node->next;
}

trs_ListNode *trs_list_node_next_mut(trs_ListNode *node) {
    ASSERT_NODE(node);

    return node->next;
}

const trs_ListNode *trs_list_node_prev(const trs_ListNode *node) {
    ASSERT_NODE(node);

    return node->prev;
}

trs_ListNode *trs_list_node_prev_mut(trs_ListNode *node) {
    ASSERT_NODE(node);

    return node->prev;
}

const trs_ListNode *trs_list_find(const trs_List *self, const void *key, trs_Eq eq) {
    ASSERT_LIST(self);
    assert(key);
    assert(eq);

    return find_node(self, key, eq);
}

trs_ListNode *trs_list_find_mut(trs_List *self, const void *key, trs_Eq eq) {
    ASSERT_LIST(self);
    assert(key);
    assert(eq);

    return find_node(self, key, eq);
}

const void *trs_list_node_elem(const trs_ListNode *node) {
    ASSERT_NODE(node);

    return node->elem;
}

void *trs_list_node_elem_mut(trs_ListNode *node) {
    ASSERT_NODE(node);

    return node->elem;
}

/* ========== mods ========== */

trs_Status trs_list_push_front(trs_List *self, const void *val) {
    ASSERT_LIST(self);
    assert(val);

    return insert_between(self, nullptr, self->head, val);
}

trs_Status trs_list_push_back(trs_List *self, const void *val) {
    ASSERT_LIST(self);
    assert(val);

    return insert_between(self, self->tail, nullptr, val);
}

void trs_list_pop_front(trs_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    remove_node(self, self->head);
}

void trs_list_pop_back(trs_List *self) {
    ASSERT_LIST(self);
    assert(self->len > 0);

    remove_node(self, self->tail);
}

trs_Status trs_list_insert_before(trs_List *self, trs_ListNode *at, const void *val) {
    ASSERT_LIST(self);
    ASSERT_NODE(at);
    assert(owns_node(self, at));
    assert(val);

    return insert_between(self, at->prev, at, val);
}

trs_Status trs_list_insert_after(trs_List *self, trs_ListNode *at, const void *val) {
    ASSERT_LIST(self);
    ASSERT_NODE(at);
    assert(owns_node(self, at));
    assert(val);

    return insert_between(self, at, at->next, val);
}

void trs_list_remove(trs_List *self, trs_ListNode *node) {
    ASSERT_LIST(self);
    ASSERT_NODE(node);
    assert(owns_node(self, node));

    remove_node(self, node);
}

void trs_list_clear(trs_List *self) {
    ASSERT_LIST(self);

    clear_nodes(self);

    ASSERT_LIST(self);
}

trs_Status trs_list_splice_front(trs_List *self, trs_List *src) {
    ASSERT_LIST(self);
    ASSERT_LIST(src);
    assert(self != src);
    assert(self->elem_size == src->elem_size);

    if (src->len == 0) {
        return TRS_STATUS_OK;
    }

    if (self->al == src->al) {
        splice_nodes(self, src, true);
        return TRS_STATUS_OK;
    }

    trs_List *copy;
    const trs_Status st = trs_list_copy_with(src, self->al, &copy);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    splice_nodes(self, copy, true);
    trs_list_drop(copy);
    clear_nodes(src);

    ASSERT_LIST(self);
    ASSERT_LIST(src);

    return TRS_STATUS_OK;
}

trs_Status trs_list_splice_back(trs_List *self, trs_List *src) {
    ASSERT_LIST(self);
    ASSERT_LIST(src);
    assert(self != src);
    assert(self->elem_size == src->elem_size);

    if (src->len == 0) {
        return TRS_STATUS_OK;
    }

    if (self->al == src->al) {
        splice_nodes(self, src, false);
        return TRS_STATUS_OK;
    }

    trs_List *copy;
    const trs_Status st = trs_list_copy_with(src, self->al, &copy);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    splice_nodes(self, copy, false);
    trs_list_drop(copy);
    clear_nodes(src);

    ASSERT_LIST(self);
    ASSERT_LIST(src);

    return TRS_STATUS_OK;
}

void trs_list_swap(trs_List *self, trs_List *other) {
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

trs_Status trs_list_splice_node(trs_List *self, trs_ListNode *at, trs_List *src, trs_ListNode *node) {
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
        trs_ListNode *prev = at ? at->prev : self->tail;
        const trs_Status st = insert_between(self, prev, at, node->elem);
        if (TRS_STATUS_IS_ERR(st)) {
            return st;
        }

        remove_node(src, node);

        ASSERT_LIST(self);
        ASSERT_LIST(src);

        return TRS_STATUS_OK;
    }

    // unlinking first is what makes 'self == src' work: 'at->prev' is read from a list
    // that no longer holds 'node', so moving a node one step forward lands where it must
    unlink_node(src, node);
    link_node(self, node, at ? at->prev : self->tail, at);

    ASSERT_LIST(self);
    ASSERT_LIST(src);
    ASSERT_NODE(node);

    return TRS_STATUS_OK;
}

/* ========== relink ========== */

void trs_list_reverse(trs_List *self) {
    ASSERT_LIST(self);

    trs_ListNode *cur = self->head;
    while (cur) {
        trs_ListNode *next = cur->next;
        TRS_SWAP(cur->next, cur->prev);
        cur = next;
    }

    TRS_SWAP(self->head, self->tail);

    ASSERT_LIST(self);
}

void trs_list_sort(trs_List *self, trs_Cmp cmp) {
    ASSERT_LIST(self);
    assert(cmp);

    if (self->len < 2) {
        return;
    }

    self->head = sort_chain(self->head, self->len, cmp);
    relink_prev(self);

    ASSERT_LIST(self);
}

trs_Status trs_list_merge(trs_List *self, trs_List *src, trs_Cmp cmp) {
    ASSERT_LIST(self);
    ASSERT_LIST(src);
    assert(self != src);
    assert(self->elem_size == src->elem_size);
    assert(cmp);

    if (src->len == 0) {
        return TRS_STATUS_OK;
    }

    if (self->al == src->al) {
        merge_into(self, src, cmp);
        return TRS_STATUS_OK;
    }

    trs_List *copy;
    const trs_Status st = trs_list_copy_with(src, self->al, &copy);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    merge_into(self, copy, cmp);
    trs_list_drop(copy);
    clear_nodes(src);

    ASSERT_LIST(self);
    ASSERT_LIST(src);

    return TRS_STATUS_OK;
}

/* ========== copy to span ========== */

void trs_list_copy_to_span(const trs_List *self, trs_SpanMut dst) {
    ASSERT_LIST(self);
    TRS_SPAN_ASSERT(dst);
    assert(dst.elem_size == self->elem_size);
    assert(dst.len == self->len);

    size_t i = 0;
    for (const trs_ListNode *node = self->head; node; node = node->next, ++i) {
        memcpy(trs_byte_offset_mut(dst.data, self->elem_size, i), node->elem, self->elem_size);
    }
}

void trs_list_copy_from_span(trs_List *self, trs_Span src) {
    ASSERT_LIST(self);
    TRS_SPAN_ASSERT(src);
    assert(src.elem_size == self->elem_size);
    assert(src.len == self->len);

    size_t i = 0;
    for (trs_ListNode *node = self->head; node; node = node->next, ++i) {
        memcpy(node->elem, trs_byte_offset(src.data, self->elem_size, i), self->elem_size);
    }
}

/* ========== print ========== */

void trs_list_fprint(const trs_List *self, FILE *stream, trs_FPrint fprint) {
    ASSERT_LIST(self);
    assert(stream);
    assert(fprint);

    fputc('[', stream);
    for (const trs_ListNode *node = self->head; node; node = node->next) {
        if (node != self->head) {
            fputs(", ", stream);
        }
        fprint(stream, node->elem);
    }
    fputs("]\n", stream);
}

void trs_list_print(const trs_List *self, trs_FPrint fprint) {
    ASSERT_LIST(self);
    assert(fprint);

    trs_list_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

static size_t node_bytes(size_t elem_size) {
    return sizeof(trs_ListNode) + elem_size;
}

static trs_Status node_new(trs_Al *al, size_t elem_size, const void *val, trs_ListNode **out) {
    assert(al);
    assert(elem_size > 0);
    assert(val);
    assert(out);

    size_t bytes;
    if (ckd_add(&bytes, sizeof(trs_ListNode), elem_size)) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    trs_ListNode *node = trs_alloc(al, bytes);
    if (!node) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    assert(trs_ptr_is_aligned(node, alignof(max_align_t)));

    node->next = nullptr;
    node->prev = nullptr;
    memcpy(node->elem, val, elem_size);

    *out = node;

    return TRS_STATUS_OK;
}

static void node_drop(trs_Al *al, size_t elem_size, trs_ListNode *node) {
    trs_dealloc(al, node, node_bytes(elem_size));
}

static void link_node(trs_List *self, trs_ListNode *node, trs_ListNode *prev, trs_ListNode *next) {
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
static trs_Status insert_between(trs_List *self, trs_ListNode *prev, trs_ListNode *next, const void *val) {
    trs_ListNode *node;
    const trs_Status st = node_new(self->al, self->elem_size, val, &node);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    link_node(self, node, prev, next);

    ASSERT_LIST(self);
    ASSERT_NODE(node);

    return TRS_STATUS_OK;
}

static void unlink_node(trs_List *self, trs_ListNode *node) {
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

static void remove_node(trs_List *self, trs_ListNode *node) {
    unlink_node(self, node);
    node_drop(self->al, self->elem_size, node);

    ASSERT_LIST(self);
}

static void splice_nodes(trs_List *self, trs_List *src, bool front) {
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

static void swap_contents(trs_List *a, trs_List *b) {
    TRS_SWAP(a->head, b->head);
    TRS_SWAP(a->tail, b->tail);
    TRS_SWAP(a->len, b->len);
}

static trs_ListNode *find_node(const trs_List *self, const void *key, trs_Eq eq) {
    for (trs_ListNode *node = self->head; node; node = node->next) {
        if (eq(node->elem, key)) {
            return node;
        }
    }

    return nullptr;
}

static void clear_nodes(trs_List *self) {
    trs_ListNode *node = self->head;
    while (node) {
        trs_ListNode *next = node->next;
        node_drop(self->al, self->elem_size, node);
        node = next;
    }

    self->head = nullptr;
    self->tail = nullptr;
    self->len = 0;
}

static trs_ListNode *merge_chains(trs_ListNode *a, trs_ListNode *b, trs_Cmp cmp) {
    assert(cmp);

    trs_ListNode *head = nullptr;
    trs_ListNode **tail = &head;

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

static trs_ListNode *sort_chain(trs_ListNode *head, size_t len, trs_Cmp cmp) {
    assert(head);
    assert(cmp);

    if (len < 2) {
        return head;
    }

    const size_t half = len / 2;

    // walk to the LAST node of the left half, so the chain can be cut behind it
    trs_ListNode *left_tail = head;
    for (size_t i = 1; i < half; ++i) {
        left_tail = left_tail->next;
    }

    trs_ListNode *right = left_tail->next;
    left_tail->next = nullptr;

    return merge_chains(sort_chain(head, half, cmp), sort_chain(right, len - half, cmp), cmp);
}

static void relink_prev(trs_List *self) {
    trs_ListNode *prev = nullptr;

    for (trs_ListNode *node = self->head; node; node = node->next) {
        node->prev = prev;
        prev = node;
    }

    self->tail = prev;
}

static void merge_into(trs_List *self, trs_List *src, trs_Cmp cmp) {
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

static bool owns_node(const trs_List *self, const trs_ListNode *node) {
    for (const trs_ListNode *cur = self->head; cur; cur = cur->next) {
        if (cur == node) {
            return true;
        }
    }
    return false;
}
