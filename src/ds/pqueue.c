#include "trs/ds/pqueue.h"

#include "trs/algo/heap.h"
#include "trs/core/util.h"
#include "trs/ds/vec.h"

#include <assert.h>

/* ========== internals ========== */

#define ASSERT_PQUEUE(q) \
    (assert(q),          \
     assert((q)->vec),   \
     assert((q)->cmp))

// A queue is a vec plus the order it is kept in: the elems live in the vec's buffer and
// every mutation leaves algo/heap's invariant standing over that buffer. Reusing the vec
// is what keeps the growth policy, the allocator handling and the copy semantics in one
// place instead of two.
struct trs_PQueue {
    trs_Vec *vec;
    trs_Cmp cmp;
};

/// takes ownership of 'vec' either way: on failure it is dropped, not handed back
[[nodiscard]]
static trs_Status wrap(trs_Vec *vec, trs_Cmp cmp, trs_PQueue **out);

/* ========== lifetime ========== */

trs_Status trs_pqueue_new(size_t elem_size, trs_Cmp cmp, trs_Al *al, trs_PQueue **out) {
    assert(elem_size > 0);
    assert(cmp);
    assert(al);
    assert(out);

    trs_Vec *vec;
    const trs_Status st = trs_vec_new(elem_size, al, &vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, cmp, out);
}

trs_Status trs_pqueue_new_cap(size_t cap, size_t elem_size, trs_Cmp cmp, trs_Al *al, trs_PQueue **out) {
    assert(elem_size > 0);
    assert(cmp);
    assert(al);
    assert(out);

    trs_Vec *vec;
    const trs_Status st = trs_vec_new_cap(cap, elem_size, al, &vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, cmp, out);
}

trs_Status trs_pqueue_from_data(
    const void *data, size_t len, size_t elem_size,
    trs_Cmp cmp,
    trs_Al *al,
    trs_PQueue **out
) {
    assert(elem_size > 0);
    assert(cmp);
    assert(al);
    assert(out);

    trs_Vec *vec;
    const trs_Status st = trs_vec_from_data(data, len, elem_size, al, &vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    trs_span_make_heap(trs_vec_to_span_mut(vec), cmp);

    return wrap(vec, cmp, out);
}

trs_Status trs_pqueue_from_span(trs_Span s, trs_Cmp cmp, trs_Al *al, trs_PQueue **out) {
    TRS_SPAN_ASSERT(s);
    assert(cmp);
    assert(al);
    assert(out);

    return trs_pqueue_from_data(s.data, s.len, s.elem_size, cmp, al, out);
}

void trs_pqueue_drop(trs_PQueue *self) {
    if (!self) {
        return;
    }

    ASSERT_PQUEUE(self);

    trs_Al *al_copy = trs_vec_al(self->vec);
    trs_vec_drop(self->vec);
    trs_dealloc(al_copy, self, sizeof(trs_PQueue));
}

trs_Vec *trs_pqueue_into_vec(trs_PQueue *self) {
    ASSERT_PQUEUE(self);

    trs_Vec *vec = self->vec;
    trs_dealloc(trs_vec_al(vec), self, sizeof(trs_PQueue));

    return vec;
}

/* ========== copy ========== */

trs_Status trs_pqueue_copy(const trs_PQueue *self, trs_PQueue **out) {
    ASSERT_PQUEUE(self);

    return trs_pqueue_copy_with(self, trs_vec_al(self->vec), out);
}

trs_Status trs_pqueue_copy_with(const trs_PQueue *self, trs_Al *al, trs_PQueue **out) {
    ASSERT_PQUEUE(self);
    assert(al);
    assert(out);

    trs_Vec *vec;
    const trs_Status st = trs_vec_copy_with(self->vec, al, &vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // the buffer is copied as it stands, heap order and all, so no reheapifying
    return wrap(vec, self->cmp, out);
}

trs_Status trs_pqueue_copy_assign(const trs_PQueue *self, trs_PQueue *other) {
    ASSERT_PQUEUE(self);
    ASSERT_PQUEUE(other);
    assert(trs_vec_elem_size(self->vec) == trs_vec_elem_size(other->vec));

    if (self == other) {
        return TRS_STATUS_OK;
    }

    const trs_Status st = trs_vec_copy_assign(self->vec, other->vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // keeping other's own comparator would leave it holding a buffer that is a heap
    // under nobody's order
    other->cmp = self->cmp;

    ASSERT_PQUEUE(other);

    return TRS_STATUS_OK;
}

trs_Status trs_pqueue_move_assign(trs_PQueue *self, trs_PQueue *other) {
    ASSERT_PQUEUE(self);
    ASSERT_PQUEUE(other);
    assert(trs_vec_elem_size(self->vec) == trs_vec_elem_size(other->vec));

    if (self == other) {
        return TRS_STATUS_OK;
    }

    const trs_Status st = trs_vec_move_assign(self->vec, other->vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // the elems arrive arranged under the comparator of 'self', so it travels with them —
    // the same reason copy_assign hands it over
    other->cmp = self->cmp;

    ASSERT_PQUEUE(other);

    return TRS_STATUS_OK;
}

/* ========== info ========== */

size_t trs_pqueue_len(const trs_PQueue *self) {
    ASSERT_PQUEUE(self);

    return trs_vec_len(self->vec);
}

size_t trs_pqueue_cap(const trs_PQueue *self) {
    ASSERT_PQUEUE(self);

    return trs_vec_cap(self->vec);
}

size_t trs_pqueue_elem_size(const trs_PQueue *self) {
    ASSERT_PQUEUE(self);

    return trs_vec_elem_size(self->vec);
}

trs_Al *trs_pqueue_al(const trs_PQueue *self) {
    ASSERT_PQUEUE(self);

    return trs_vec_al(self->vec);
}

trs_Cmp trs_pqueue_cmp(const trs_PQueue *self) {
    ASSERT_PQUEUE(self);

    return self->cmp;
}

/* ========== access ========== */

const void *trs_pqueue_top(const trs_PQueue *self) {
    ASSERT_PQUEUE(self);
    assert(trs_vec_len(self->vec) > 0);

    return trs_vec_front(self->vec);
}

/* ========== mods ========== */

trs_Status trs_pqueue_push(trs_PQueue *self, const void *val) {
    ASSERT_PQUEUE(self);
    assert(val);

    const trs_Status st = trs_vec_push(self->vec, val);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // the new elem sits last, which is exactly where push_heap expects it
    trs_span_push_heap(trs_vec_to_span_mut(self->vec), self->cmp);

    return TRS_STATUS_OK;
}

void trs_pqueue_pop(trs_PQueue *self) {
    ASSERT_PQUEUE(self);
    assert(trs_vec_len(self->vec) > 0);

    // pop_heap parks the greatest elem last and leaves a heap in front of it; dropping
    // the tail is then the vec's business
    trs_span_pop_heap(trs_vec_to_span_mut(self->vec), self->cmp);
    trs_vec_pop(self->vec);
}

void trs_pqueue_clear(trs_PQueue *self) {
    ASSERT_PQUEUE(self);

    trs_vec_clear(self->vec);
}

trs_Status trs_pqueue_reserve(trs_PQueue *self, size_t new_cap) {
    ASSERT_PQUEUE(self);

    return trs_vec_reserve(self->vec, new_cap);
}

trs_Status trs_pqueue_shrink_to_fit(trs_PQueue *self) {
    ASSERT_PQUEUE(self);

    return trs_vec_shrink_to_fit(self->vec);
}

void trs_pqueue_swap(trs_PQueue *self, trs_PQueue *other) {
    ASSERT_PQUEUE(self);
    ASSERT_PQUEUE(other);
    assert(trs_vec_elem_size(self->vec) == trs_vec_elem_size(other->vec));

    if (self == other) {
        return;
    }

    trs_vec_swap(self->vec, other->vec);
    TRS_SWAP(self->cmp, other->cmp);
}

/* ========== to span ========== */

trs_Span trs_pqueue_to_span(const trs_PQueue *self) {
    ASSERT_PQUEUE(self);

    return trs_vec_to_span(self->vec);
}

/* ========== print ========== */

void trs_pqueue_fprint(const trs_PQueue *self, FILE *stream, trs_FPrint fprint) {
    ASSERT_PQUEUE(self);

    trs_vec_fprint(self->vec, stream, fprint);
}

void trs_pqueue_print(const trs_PQueue *self, trs_FPrint fprint) {
    ASSERT_PQUEUE(self);

    trs_vec_print(self->vec, fprint);
}

/* ========== internals ========== */

static trs_Status wrap(trs_Vec *vec, trs_Cmp cmp, trs_PQueue **out) {
    assert(vec);
    assert(cmp);
    assert(out);

    trs_PQueue *obj = trs_alloc(trs_vec_al(vec), sizeof(trs_PQueue));
    if (!obj) {
        trs_vec_drop(vec);
        return TRS_STATUS_ERR_NO_MEM;
    }

    obj->vec = vec;
    obj->cmp = cmp;

    *out = obj;

    return TRS_STATUS_OK;
}
