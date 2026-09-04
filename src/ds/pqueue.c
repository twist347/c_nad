#include "nad/ds/pqueue.h"

#include "nad/algo/heap.h"
#include "nad/core/util.h"
#include "nad/ds/vec.h"

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
struct nad_PQueue {
    nad_Vec *vec;
    nad_Cmp cmp;
};

/// takes ownership of 'vec' either way: on failure it is dropped, not handed back
[[nodiscard]]
static nad_Status wrap(nad_Vec *vec, nad_Cmp cmp, nad_PQueue **out);

/* ========== lifetime ========== */

nad_Status nad_pqueue_new(size_t elem_size, nad_Cmp cmp, nad_Al *al, nad_PQueue **out) {
    assert(elem_size > 0);
    assert(cmp);
    assert(al);
    assert(out);

    nad_Vec *vec;
    const nad_Status st = nad_vec_new(elem_size, al, &vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, cmp, out);
}

nad_Status nad_pqueue_new_cap(size_t cap, size_t elem_size, nad_Cmp cmp, nad_Al *al, nad_PQueue **out) {
    assert(elem_size > 0);
    assert(cmp);
    assert(al);
    assert(out);

    nad_Vec *vec;
    const nad_Status st = nad_vec_new_cap(cap, elem_size, al, &vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, cmp, out);
}

nad_Status nad_pqueue_from_data(const void *data, size_t len, size_t elem_size, nad_Cmp cmp, nad_Al *al, nad_PQueue **out) {
    assert(elem_size > 0);
    assert(cmp);
    assert(al);
    assert(out);

    nad_Vec *vec;
    const nad_Status st = nad_vec_from_data(data, len, elem_size, al, &vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    nad_span_make_heap(nad_vec_to_span_mut(vec), cmp);

    return wrap(vec, cmp, out);
}

nad_Status nad_pqueue_from_span(nad_Span s, nad_Cmp cmp, nad_Al *al, nad_PQueue **out) {
    NAD_SPAN_ASSERT(s);
    assert(cmp);
    assert(al);
    assert(out);

    return nad_pqueue_from_data(s.data, s.len, s.elem_size, cmp, al, out);
}

void nad_pqueue_drop(nad_PQueue *self) {
    if (!self) {
        return;
    }

    ASSERT_PQUEUE(self);

    nad_Al *al_copy = nad_vec_al(self->vec);
    nad_vec_drop(self->vec);
    nad_dealloc(al_copy, self, sizeof(nad_PQueue));
}

nad_Vec *nad_pqueue_into_vec(nad_PQueue *self) {
    ASSERT_PQUEUE(self);

    nad_Vec *vec = self->vec;
    nad_dealloc(nad_vec_al(vec), self, sizeof(nad_PQueue));

    return vec;
}

/* ========== copy ========== */

nad_Status nad_pqueue_copy(const nad_PQueue *self, nad_PQueue **out) {
    ASSERT_PQUEUE(self);

    return nad_pqueue_copy_with(self, nad_vec_al(self->vec), out);
}

nad_Status nad_pqueue_copy_with(const nad_PQueue *self, nad_Al *al, nad_PQueue **out) {
    ASSERT_PQUEUE(self);
    assert(al);
    assert(out);

    nad_Vec *vec;
    const nad_Status st = nad_vec_copy_with(self->vec, al, &vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    // the buffer is copied as it stands, heap order and all, so no reheapifying
    return wrap(vec, self->cmp, out);
}

nad_Status nad_pqueue_copy_assign(const nad_PQueue *self, nad_PQueue *other) {
    ASSERT_PQUEUE(self);
    ASSERT_PQUEUE(other);
    assert(nad_vec_elem_size(self->vec) == nad_vec_elem_size(other->vec));

    if (self == other) {
        return NAD_STATUS_OK;
    }

    const nad_Status st = nad_vec_copy_assign(self->vec, other->vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    // keeping other's own comparator would leave it holding a buffer that is a heap
    // under nobody's order
    other->cmp = self->cmp;

    ASSERT_PQUEUE(other);

    return NAD_STATUS_OK;
}

nad_Status nad_pqueue_move_assign(nad_PQueue *self, nad_PQueue *other) {
    ASSERT_PQUEUE(self);
    ASSERT_PQUEUE(other);
    assert(nad_vec_elem_size(self->vec) == nad_vec_elem_size(other->vec));

    if (self == other) {
        return NAD_STATUS_OK;
    }

    const nad_Status st = nad_vec_move_assign(self->vec, other->vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    // the elems arrive arranged under the comparator of 'self', so it travels with them —
    // the same reason copy_assign hands it over
    other->cmp = self->cmp;

    ASSERT_PQUEUE(other);

    return NAD_STATUS_OK;
}

/* ========== info ========== */

size_t nad_pqueue_len(const nad_PQueue *self) {
    ASSERT_PQUEUE(self);

    return nad_vec_len(self->vec);
}

size_t nad_pqueue_cap(const nad_PQueue *self) {
    ASSERT_PQUEUE(self);

    return nad_vec_cap(self->vec);
}

size_t nad_pqueue_elem_size(const nad_PQueue *self) {
    ASSERT_PQUEUE(self);

    return nad_vec_elem_size(self->vec);
}

nad_Al *nad_pqueue_al(const nad_PQueue *self) {
    ASSERT_PQUEUE(self);

    return nad_vec_al(self->vec);
}

nad_Cmp nad_pqueue_cmp(const nad_PQueue *self) {
    ASSERT_PQUEUE(self);

    return self->cmp;
}

/* ========== access ========== */

const void *nad_pqueue_top(const nad_PQueue *self) {
    ASSERT_PQUEUE(self);
    assert(nad_vec_len(self->vec) > 0);

    return nad_vec_first(self->vec);
}

/* ========== mods ========== */

nad_Status nad_pqueue_push(nad_PQueue *self, const void *val) {
    ASSERT_PQUEUE(self);
    assert(val);

    const nad_Status st = nad_vec_push(self->vec, val);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    // the new elem sits last, which is exactly where push_heap expects it
    nad_span_push_heap(nad_vec_to_span_mut(self->vec), self->cmp);

    return NAD_STATUS_OK;
}

void nad_pqueue_pop(nad_PQueue *self) {
    ASSERT_PQUEUE(self);
    assert(nad_vec_len(self->vec) > 0);

    // pop_heap parks the greatest elem last and leaves a heap in front of it; dropping
    // the tail is then the vec's business
    nad_span_pop_heap(nad_vec_to_span_mut(self->vec), self->cmp);
    nad_vec_pop(self->vec);
}

void nad_pqueue_clear(nad_PQueue *self) {
    ASSERT_PQUEUE(self);

    nad_vec_clear(self->vec);
}

nad_Status nad_pqueue_reserve(nad_PQueue *self, size_t new_cap) {
    ASSERT_PQUEUE(self);

    return nad_vec_reserve(self->vec, new_cap);
}

nad_Status nad_pqueue_shrink_to_fit(nad_PQueue *self) {
    ASSERT_PQUEUE(self);

    return nad_vec_shrink_to_fit(self->vec);
}

nad_Status nad_pqueue_swap(nad_PQueue *self, nad_PQueue *other) {
    ASSERT_PQUEUE(self);
    ASSERT_PQUEUE(other);
    assert(nad_vec_elem_size(self->vec) == nad_vec_elem_size(other->vec));

    if (self == other) {
        return NAD_STATUS_OK;
    }

    const nad_Status st = nad_vec_swap(self->vec, other->vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    NAD_SWAP(self->cmp, other->cmp);

    return NAD_STATUS_OK;
}

/* ========== to span ========== */

nad_Span nad_pqueue_to_span(const nad_PQueue *self) {
    ASSERT_PQUEUE(self);

    return nad_vec_to_span(self->vec);
}

/* ========== print ========== */

void nad_pqueue_fprint(const nad_PQueue *self, FILE *stream, nad_FPrint fprint) {
    ASSERT_PQUEUE(self);

    nad_vec_fprint(self->vec, stream, fprint);
}

void nad_pqueue_print(const nad_PQueue *self, nad_FPrint fprint) {
    ASSERT_PQUEUE(self);

    nad_vec_print(self->vec, fprint);
}

/* ========== internals ========== */

static nad_Status wrap(nad_Vec *vec, nad_Cmp cmp, nad_PQueue **out) {
    assert(vec);
    assert(cmp);
    assert(out);

    nad_PQueue *obj = nad_alloc(nad_vec_al(vec), sizeof(nad_PQueue));
    if (!obj) {
        nad_vec_drop(vec);
        return NAD_STATUS_ERR_NO_MEM;
    }

    obj->vec = vec;
    obj->cmp = cmp;

    *out = obj;

    return NAD_STATUS_OK;
}
