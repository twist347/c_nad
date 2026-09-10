#include "trs/ds/queue.h"

#include "trs/ds/deque.h"

#include <assert.h>

/* ========== internals ========== */

#define ASSERT_QUEUE(q) \
    (assert(q),         \
     assert((q)->deque))

// A queue is a deque seen through a smaller keyhole: the elems live in the deque's ring
// and every operation here is one of the deque's, renamed to the end it acts on. Reusing
// it keeps the growth policy, the allocator handling and the copy semantics in one place
// instead of two — what this type contributes is the operations it does NOT forward.
struct trs_Queue {
    trs_Deque *deque;
};

/// takes ownership of 'deque' either way: on failure it is dropped, not handed back
[[nodiscard]]
static trs_Status wrap(trs_Deque *deque, trs_Queue **out);

/* ========== lifetime ========== */

trs_Status trs_queue_new(size_t elem_size, trs_Al *al, trs_Queue **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Deque *deque;
    const trs_Status st = trs_deque_new(elem_size, al, &deque);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(deque, out);
}

trs_Status trs_queue_new_cap(size_t cap, size_t elem_size, trs_Al *al, trs_Queue **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Deque *deque;
    const trs_Status st = trs_deque_new_cap(cap, elem_size, al, &deque);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(deque, out);
}

trs_Status trs_queue_from_data(const void *data, size_t len, size_t elem_size, trs_Al *al, trs_Queue **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Deque *deque;
    const trs_Status st = trs_deque_from_data(data, len, elem_size, al, &deque);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // the deque lays them out front to back, which is arrival order already
    return wrap(deque, out);
}

trs_Status trs_queue_from_span(trs_Span s, trs_Al *al, trs_Queue **out) {
    TRS_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return trs_queue_from_data(s.data, s.len, s.elem_size, al, out);
}

void trs_queue_drop(trs_Queue *self) {
    if (!self) {
        return;
    }

    ASSERT_QUEUE(self);

    trs_Al *al_copy = trs_deque_al(self->deque);
    trs_deque_drop(self->deque);
    trs_dealloc(al_copy, self, sizeof(trs_Queue));
}

trs_Deque *trs_queue_into_deque(trs_Queue *self) {
    ASSERT_QUEUE(self);

    trs_Deque *deque = self->deque;
    trs_dealloc(trs_deque_al(deque), self, sizeof(trs_Queue));

    return deque;
}

/* ========== copy ========== */

trs_Status trs_queue_copy(const trs_Queue *self, trs_Queue **out) {
    ASSERT_QUEUE(self);

    return trs_queue_copy_with(self, trs_deque_al(self->deque), out);
}

trs_Status trs_queue_copy_with(const trs_Queue *self, trs_Al *al, trs_Queue **out) {
    ASSERT_QUEUE(self);
    assert(al);
    assert(out);

    trs_Deque *deque;
    const trs_Status st = trs_deque_copy_with(self->deque, al, &deque);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(deque, out);
}

trs_Status trs_queue_copy_assign(const trs_Queue *self, trs_Queue *other) {
    ASSERT_QUEUE(self);
    ASSERT_QUEUE(other);
    assert(trs_deque_elem_size(self->deque) == trs_deque_elem_size(other->deque));

    // self assignment is left to the deque, which already returns early on it: a guard
    // repeated here would be a branch no test could tell from its absence
    return trs_deque_copy_assign(self->deque, other->deque);
}

trs_Status trs_queue_move_assign(trs_Queue *self, trs_Queue *other) {
    ASSERT_QUEUE(self);
    ASSERT_QUEUE(other);
    assert(trs_deque_elem_size(self->deque) == trs_deque_elem_size(other->deque));

    // as in copy_assign, moving a queue onto itself is the deque's early return
    return trs_deque_move_assign(self->deque, other->deque);
}

void trs_queue_copy_to_span(const trs_Queue *self, trs_SpanMut dst) {
    ASSERT_QUEUE(self);
    TRS_SPAN_ASSERT(dst);

    trs_deque_copy_to_span(self->deque, dst);
}

/* ========== compare ========== */

bool trs_queue_eq(const trs_Queue *a, const trs_Queue *b) {
    ASSERT_QUEUE(a);
    ASSERT_QUEUE(b);

    return trs_deque_eq(a->deque, b->deque);
}

bool trs_queue_eq_by(const trs_Queue *a, const trs_Queue *b, trs_Eq eq) {
    ASSERT_QUEUE(a);
    ASSERT_QUEUE(b);

    return trs_deque_eq_by(a->deque, b->deque, eq);
}

/* ========== info ========== */

size_t trs_queue_len(const trs_Queue *self) {
    ASSERT_QUEUE(self);

    return trs_deque_len(self->deque);
}

size_t trs_queue_cap(const trs_Queue *self) {
    ASSERT_QUEUE(self);

    return trs_deque_cap(self->deque);
}

size_t trs_queue_elem_size(const trs_Queue *self) {
    ASSERT_QUEUE(self);

    return trs_deque_elem_size(self->deque);
}

trs_Al *trs_queue_al(const trs_Queue *self) {
    ASSERT_QUEUE(self);

    return trs_deque_al(self->deque);
}

/* ========== access ========== */

const void *trs_queue_front(const trs_Queue *self) {
    ASSERT_QUEUE(self);
    assert(trs_deque_len(self->deque) > 0);

    return trs_deque_front(self->deque);
}

void *trs_queue_front_mut(trs_Queue *self) {
    ASSERT_QUEUE(self);
    assert(trs_deque_len(self->deque) > 0);

    return trs_deque_front_mut(self->deque);
}

const void *trs_queue_back(const trs_Queue *self) {
    ASSERT_QUEUE(self);
    assert(trs_deque_len(self->deque) > 0);

    return trs_deque_back(self->deque);
}

void *trs_queue_back_mut(trs_Queue *self) {
    ASSERT_QUEUE(self);
    assert(trs_deque_len(self->deque) > 0);

    return trs_deque_back_mut(self->deque);
}

/* ========== mods ========== */

trs_Status trs_queue_push(trs_Queue *self, const void *val) {
    ASSERT_QUEUE(self);
    assert(val);

    return trs_deque_push_back(self->deque, val);
}

void trs_queue_pop(trs_Queue *self) {
    ASSERT_QUEUE(self);
    assert(trs_deque_len(self->deque) > 0);

    trs_deque_pop_front(self->deque);
}

void trs_queue_clear(trs_Queue *self) {
    ASSERT_QUEUE(self);

    trs_deque_clear(self->deque);
}

trs_Status trs_queue_reserve(trs_Queue *self, size_t new_cap) {
    ASSERT_QUEUE(self);

    return trs_deque_reserve(self->deque, new_cap);
}

trs_Status trs_queue_shrink_to_fit(trs_Queue *self) {
    ASSERT_QUEUE(self);

    return trs_deque_shrink_to_fit(self->deque);
}

void trs_queue_swap(trs_Queue *self, trs_Queue *other) {
    ASSERT_QUEUE(self);
    ASSERT_QUEUE(other);
    assert(trs_deque_elem_size(self->deque) == trs_deque_elem_size(other->deque));

    // as in copy_assign, swapping a queue with itself is the deque's early return
    trs_deque_swap(self->deque, other->deque);
}

/* ========== print ========== */

void trs_queue_fprint(const trs_Queue *self, FILE *stream, trs_FPrint fprint) {
    ASSERT_QUEUE(self);

    trs_deque_fprint(self->deque, stream, fprint);
}

void trs_queue_print(const trs_Queue *self, trs_FPrint fprint) {
    ASSERT_QUEUE(self);

    trs_deque_print(self->deque, fprint);
}

/* ========== internals ========== */

static trs_Status wrap(trs_Deque *deque, trs_Queue **out) {
    assert(deque);
    assert(out);

    trs_Queue *obj = trs_alloc(trs_deque_al(deque), sizeof(trs_Queue));
    if (!obj) {
        trs_deque_drop(deque);
        return TRS_STATUS_ERR_NO_MEM;
    }

    obj->deque = deque;

    *out = obj;

    return TRS_STATUS_OK;
}
