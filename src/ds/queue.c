#include "nad/ds/queue.h"

#include "nad/ds/deque.h"

#include <assert.h>

/* ========== internals ========== */

#define ASSERT_QUEUE(q) \
    (assert(q),         \
     assert((q)->deque))

// A queue is a deque seen through a smaller keyhole: the elems live in the deque's ring
// and every operation here is one of the deque's, renamed to the end it acts on. Reusing
// it keeps the growth policy, the allocator handling and the copy semantics in one place
// instead of two — what this type contributes is the operations it does NOT forward.
struct nad_Queue {
    nad_Deque *deque;
};

/// takes ownership of 'deque' either way: on failure it is dropped, not handed back
[[nodiscard]]
static nad_Status wrap(nad_Deque *deque, nad_Queue **out);

/* ========== lifetime ========== */

nad_Status nad_queue_new(size_t elem_size, nad_Al *al, nad_Queue **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Deque *deque;
    const nad_Status st = nad_deque_new(elem_size, al, &deque);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(deque, out);
}

nad_Status nad_queue_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Queue **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Deque *deque;
    const nad_Status st = nad_deque_new_cap(cap, elem_size, al, &deque);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(deque, out);
}

nad_Status nad_queue_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Queue **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Deque *deque;
    const nad_Status st = nad_deque_from_data(data, len, elem_size, al, &deque);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    // the deque lays them out front to back, which is arrival order already
    return wrap(deque, out);
}

nad_Status nad_queue_from_span(nad_Span s, nad_Al *al, nad_Queue **out) {
    NAD_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return nad_queue_from_data(s.data, s.len, s.elem_size, al, out);
}

void nad_queue_drop(nad_Queue *self) {
    if (!self) {
        return;
    }

    ASSERT_QUEUE(self);

    nad_Al *al_copy = nad_deque_al(self->deque);
    nad_deque_drop(self->deque);
    nad_dealloc(al_copy, self, sizeof(nad_Queue));
}

nad_Deque *nad_queue_into_deque(nad_Queue *self) {
    ASSERT_QUEUE(self);

    nad_Deque *deque = self->deque;
    nad_dealloc(nad_deque_al(deque), self, sizeof(nad_Queue));

    return deque;
}

/* ========== copy ========== */

nad_Status nad_queue_copy(const nad_Queue *self, nad_Queue **out) {
    ASSERT_QUEUE(self);

    return nad_queue_copy_with(self, nad_deque_al(self->deque), out);
}

nad_Status nad_queue_copy_with(const nad_Queue *self, nad_Al *al, nad_Queue **out) {
    ASSERT_QUEUE(self);
    assert(al);
    assert(out);

    nad_Deque *deque;
    const nad_Status st = nad_deque_copy_with(self->deque, al, &deque);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(deque, out);
}

nad_Status nad_queue_copy_assign(const nad_Queue *self, nad_Queue *other) {
    ASSERT_QUEUE(self);
    ASSERT_QUEUE(other);
    assert(nad_deque_elem_size(self->deque) == nad_deque_elem_size(other->deque));

    // self assignment is left to the deque, which already returns early on it: a guard
    // repeated here would be a branch no test could tell from its absence
    return nad_deque_copy_assign(self->deque, other->deque);
}

nad_Status nad_queue_move_assign(nad_Queue *self, nad_Queue *other) {
    ASSERT_QUEUE(self);
    ASSERT_QUEUE(other);
    assert(nad_deque_elem_size(self->deque) == nad_deque_elem_size(other->deque));

    // as in copy_assign, moving a queue onto itself is the deque's early return
    return nad_deque_move_assign(self->deque, other->deque);
}

void nad_queue_copy_to_span(const nad_Queue *self, nad_SpanMut dst) {
    ASSERT_QUEUE(self);
    NAD_SPAN_ASSERT(dst);

    nad_deque_copy_to_span(self->deque, dst);
}

/* ========== compare ========== */

bool nad_queue_eq(const nad_Queue *a, const nad_Queue *b) {
    ASSERT_QUEUE(a);
    ASSERT_QUEUE(b);

    return nad_deque_eq(a->deque, b->deque);
}

bool nad_queue_eq_by(const nad_Queue *a, const nad_Queue *b, nad_Eq eq) {
    ASSERT_QUEUE(a);
    ASSERT_QUEUE(b);

    return nad_deque_eq_by(a->deque, b->deque, eq);
}

/* ========== info ========== */

size_t nad_queue_len(const nad_Queue *self) {
    ASSERT_QUEUE(self);

    return nad_deque_len(self->deque);
}

size_t nad_queue_cap(const nad_Queue *self) {
    ASSERT_QUEUE(self);

    return nad_deque_cap(self->deque);
}

size_t nad_queue_elem_size(const nad_Queue *self) {
    ASSERT_QUEUE(self);

    return nad_deque_elem_size(self->deque);
}

nad_Al *nad_queue_al(const nad_Queue *self) {
    ASSERT_QUEUE(self);

    return nad_deque_al(self->deque);
}

/* ========== access ========== */

const void *nad_queue_front(const nad_Queue *self) {
    ASSERT_QUEUE(self);
    assert(nad_deque_len(self->deque) > 0);

    return nad_deque_first(self->deque);
}

void *nad_queue_front_mut(nad_Queue *self) {
    ASSERT_QUEUE(self);
    assert(nad_deque_len(self->deque) > 0);

    return nad_deque_first_mut(self->deque);
}

const void *nad_queue_back(const nad_Queue *self) {
    ASSERT_QUEUE(self);
    assert(nad_deque_len(self->deque) > 0);

    return nad_deque_last(self->deque);
}

void *nad_queue_back_mut(nad_Queue *self) {
    ASSERT_QUEUE(self);
    assert(nad_deque_len(self->deque) > 0);

    return nad_deque_last_mut(self->deque);
}

/* ========== mods ========== */

nad_Status nad_queue_push(nad_Queue *self, const void *val) {
    ASSERT_QUEUE(self);
    assert(val);

    return nad_deque_push_back(self->deque, val);
}

void nad_queue_pop(nad_Queue *self) {
    ASSERT_QUEUE(self);
    assert(nad_deque_len(self->deque) > 0);

    nad_deque_pop_front(self->deque);
}

void nad_queue_clear(nad_Queue *self) {
    ASSERT_QUEUE(self);

    nad_deque_clear(self->deque);
}

nad_Status nad_queue_reserve(nad_Queue *self, size_t new_cap) {
    ASSERT_QUEUE(self);

    return nad_deque_reserve(self->deque, new_cap);
}

nad_Status nad_queue_shrink_to_fit(nad_Queue *self) {
    ASSERT_QUEUE(self);

    return nad_deque_shrink_to_fit(self->deque);
}

void nad_queue_swap(nad_Queue *self, nad_Queue *other) {
    ASSERT_QUEUE(self);
    ASSERT_QUEUE(other);
    assert(nad_deque_elem_size(self->deque) == nad_deque_elem_size(other->deque));

    // as in copy_assign, swapping a queue with itself is the deque's early return
    nad_deque_swap(self->deque, other->deque);
}

/* ========== print ========== */

void nad_queue_fprint(const nad_Queue *self, FILE *stream, nad_FPrint fprint) {
    ASSERT_QUEUE(self);

    nad_deque_fprint(self->deque, stream, fprint);
}

void nad_queue_print(const nad_Queue *self, nad_FPrint fprint) {
    ASSERT_QUEUE(self);

    nad_deque_print(self->deque, fprint);
}

/* ========== internals ========== */

static nad_Status wrap(nad_Deque *deque, nad_Queue **out) {
    assert(deque);
    assert(out);

    nad_Queue *obj = nad_alloc(nad_deque_al(deque), sizeof(nad_Queue));
    if (!obj) {
        nad_deque_drop(deque);
        return NAD_STATUS_ERR_NO_MEM;
    }

    obj->deque = deque;

    *out = obj;

    return NAD_STATUS_OK;
}
