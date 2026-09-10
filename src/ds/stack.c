#include "trs/ds/stack.h"

#include "trs/ds/vec.h"

#include <assert.h>

/* ========== internals ========== */

#define ASSERT_STACK(s) \
    (assert(s),         \
     assert((s)->vec))

// A stack is a vec seen through a smaller keyhole: the elems live in the vec's buffer and
// every operation here is one of the vec's, renamed to the end it acts on. Reusing it
// keeps the growth policy, the allocator handling and the copy semantics in one place
// instead of two — what this type contributes is the operations it does NOT forward.
struct trs_Stack {
    trs_Vec *vec;
};

/// takes ownership of 'vec' either way: on failure it is dropped, not handed back
[[nodiscard]]
static trs_Status wrap(trs_Vec *vec, trs_Stack **out);

/* ========== lifetime ========== */

trs_Status trs_stack_new(size_t elem_size, trs_Al *al, trs_Stack **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Vec *vec;
    const trs_Status st = trs_vec_new(elem_size, al, &vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, out);
}

trs_Status trs_stack_new_cap(size_t cap, size_t elem_size, trs_Al *al, trs_Stack **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Vec *vec;
    const trs_Status st = trs_vec_new_cap(cap, elem_size, al, &vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, out);
}

trs_Status trs_stack_from_data(const void *data, size_t len, size_t elem_size, trs_Al *al, trs_Stack **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Vec *vec;
    const trs_Status st = trs_vec_from_data(data, len, elem_size, al, &vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    // the vec lays them out in order, so the last one is the one on top
    return wrap(vec, out);
}

trs_Status trs_stack_from_span(trs_Span s, trs_Al *al, trs_Stack **out) {
    TRS_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return trs_stack_from_data(s.data, s.len, s.elem_size, al, out);
}

void trs_stack_drop(trs_Stack *self) {
    if (!self) {
        return;
    }

    ASSERT_STACK(self);

    trs_Al *al_copy = trs_vec_al(self->vec);
    trs_vec_drop(self->vec);
    trs_dealloc(al_copy, self, sizeof(trs_Stack));
}

trs_Vec *trs_stack_into_vec(trs_Stack *self) {
    ASSERT_STACK(self);

    trs_Vec *vec = self->vec;
    trs_dealloc(trs_vec_al(vec), self, sizeof(trs_Stack));

    return vec;
}

/* ========== copy ========== */

trs_Status trs_stack_copy(const trs_Stack *self, trs_Stack **out) {
    ASSERT_STACK(self);

    return trs_stack_copy_with(self, trs_vec_al(self->vec), out);
}

trs_Status trs_stack_copy_with(const trs_Stack *self, trs_Al *al, trs_Stack **out) {
    ASSERT_STACK(self);
    assert(al);
    assert(out);

    trs_Vec *vec;
    const trs_Status st = trs_vec_copy_with(self->vec, al, &vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, out);
}

trs_Status trs_stack_copy_assign(const trs_Stack *self, trs_Stack *other) {
    ASSERT_STACK(self);
    ASSERT_STACK(other);
    assert(trs_vec_elem_size(self->vec) == trs_vec_elem_size(other->vec));

    // self assignment is left to the vec, which already returns early on it: a guard
    // repeated here would be a branch no test could tell from its absence
    return trs_vec_copy_assign(self->vec, other->vec);
}

trs_Status trs_stack_move_assign(trs_Stack *self, trs_Stack *other) {
    ASSERT_STACK(self);
    ASSERT_STACK(other);
    assert(trs_vec_elem_size(self->vec) == trs_vec_elem_size(other->vec));

    // as in copy_assign, moving a stack onto itself is the vec's early return
    return trs_vec_move_assign(self->vec, other->vec);
}

/* ========== compare ========== */

bool trs_stack_eq(const trs_Stack *a, const trs_Stack *b) {
    ASSERT_STACK(a);
    ASSERT_STACK(b);

    return trs_vec_eq(a->vec, b->vec);
}

bool trs_stack_eq_by(const trs_Stack *a, const trs_Stack *b, trs_Eq eq) {
    ASSERT_STACK(a);
    ASSERT_STACK(b);

    return trs_vec_eq_by(a->vec, b->vec, eq);
}

/* ========== info ========== */

size_t trs_stack_len(const trs_Stack *self) {
    ASSERT_STACK(self);

    return trs_vec_len(self->vec);
}

size_t trs_stack_cap(const trs_Stack *self) {
    ASSERT_STACK(self);

    return trs_vec_cap(self->vec);
}

size_t trs_stack_elem_size(const trs_Stack *self) {
    ASSERT_STACK(self);

    return trs_vec_elem_size(self->vec);
}

trs_Al *trs_stack_al(const trs_Stack *self) {
    ASSERT_STACK(self);

    return trs_vec_al(self->vec);
}

/* ========== access ========== */

const void *trs_stack_top(const trs_Stack *self) {
    ASSERT_STACK(self);
    assert(trs_vec_len(self->vec) > 0);

    return trs_vec_back(self->vec);
}

void *trs_stack_top_mut(trs_Stack *self) {
    ASSERT_STACK(self);
    assert(trs_vec_len(self->vec) > 0);

    return trs_vec_back_mut(self->vec);
}

/* ========== mods ========== */

trs_Status trs_stack_push(trs_Stack *self, const void *val) {
    ASSERT_STACK(self);
    assert(val);

    return trs_vec_push(self->vec, val);
}

void trs_stack_pop(trs_Stack *self) {
    ASSERT_STACK(self);
    assert(trs_vec_len(self->vec) > 0);

    trs_vec_pop(self->vec);
}

void trs_stack_clear(trs_Stack *self) {
    ASSERT_STACK(self);

    trs_vec_clear(self->vec);
}

trs_Status trs_stack_reserve(trs_Stack *self, size_t new_cap) {
    ASSERT_STACK(self);

    return trs_vec_reserve(self->vec, new_cap);
}

trs_Status trs_stack_shrink_to_fit(trs_Stack *self) {
    ASSERT_STACK(self);

    return trs_vec_shrink_to_fit(self->vec);
}

void trs_stack_swap(trs_Stack *self, trs_Stack *other) {
    ASSERT_STACK(self);
    ASSERT_STACK(other);
    assert(trs_vec_elem_size(self->vec) == trs_vec_elem_size(other->vec));

    // as in copy_assign, swapping a stack with itself is the vec's early return
    trs_vec_swap(self->vec, other->vec);
}

/* ========== to span ========== */

trs_Span trs_stack_to_span(const trs_Stack *self) {
    ASSERT_STACK(self);

    return trs_vec_to_span(self->vec);
}

/* ========== print ========== */

void trs_stack_fprint(const trs_Stack *self, FILE *stream, trs_FPrint fprint) {
    ASSERT_STACK(self);

    trs_vec_fprint(self->vec, stream, fprint);
}

void trs_stack_print(const trs_Stack *self, trs_FPrint fprint) {
    ASSERT_STACK(self);

    trs_vec_print(self->vec, fprint);
}

/* ========== internals ========== */

static trs_Status wrap(trs_Vec *vec, trs_Stack **out) {
    assert(vec);
    assert(out);

    trs_Stack *obj = trs_alloc(trs_vec_al(vec), sizeof(trs_Stack));
    if (!obj) {
        trs_vec_drop(vec);
        return TRS_STATUS_ERR_NO_MEM;
    }

    obj->vec = vec;

    *out = obj;

    return TRS_STATUS_OK;
}
