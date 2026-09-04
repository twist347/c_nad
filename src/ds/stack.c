#include "nad/ds/stack.h"

#include "nad/ds/vec.h"

#include <assert.h>

/* ========== internals ========== */

#define ASSERT_STACK(s) \
    (assert(s),         \
     assert((s)->vec))

// A stack is a vec seen through a smaller keyhole: the elems live in the vec's buffer and
// every operation here is one of the vec's, renamed to the end it acts on. Reusing it
// keeps the growth policy, the allocator handling and the copy semantics in one place
// instead of two — what this type contributes is the operations it does NOT forward.
struct nad_Stack {
    nad_Vec *vec;
};

/// takes ownership of 'vec' either way: on failure it is dropped, not handed back
[[nodiscard]]
static nad_Status wrap(nad_Vec *vec, nad_Stack **out);

/* ========== lifetime ========== */

nad_Status nad_stack_new(size_t elem_size, nad_Al *al, nad_Stack **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Vec *vec;
    const nad_Status st = nad_vec_new(elem_size, al, &vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, out);
}

nad_Status nad_stack_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Stack **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Vec *vec;
    const nad_Status st = nad_vec_new_cap(cap, elem_size, al, &vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, out);
}

nad_Status nad_stack_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Stack **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Vec *vec;
    const nad_Status st = nad_vec_from_data(data, len, elem_size, al, &vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    // the vec lays them out in order, so the last one is the one on top
    return wrap(vec, out);
}

nad_Status nad_stack_from_span(nad_Span s, nad_Al *al, nad_Stack **out) {
    NAD_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return nad_stack_from_data(s.data, s.len, s.elem_size, al, out);
}

void nad_stack_drop(nad_Stack *self) {
    if (!self) {
        return;
    }

    ASSERT_STACK(self);

    nad_Al *al_copy = nad_vec_al(self->vec);
    nad_vec_drop(self->vec);
    nad_dealloc(al_copy, self, sizeof(nad_Stack));
}

nad_Vec *nad_stack_into_vec(nad_Stack *self) {
    ASSERT_STACK(self);

    nad_Vec *vec = self->vec;
    nad_dealloc(nad_vec_al(vec), self, sizeof(nad_Stack));

    return vec;
}

/* ========== copy ========== */

nad_Status nad_stack_copy(const nad_Stack *self, nad_Stack **out) {
    ASSERT_STACK(self);

    return nad_stack_copy_with(self, nad_vec_al(self->vec), out);
}

nad_Status nad_stack_copy_with(const nad_Stack *self, nad_Al *al, nad_Stack **out) {
    ASSERT_STACK(self);
    assert(al);
    assert(out);

    nad_Vec *vec;
    const nad_Status st = nad_vec_copy_with(self->vec, al, &vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    return wrap(vec, out);
}

nad_Status nad_stack_copy_assign(const nad_Stack *self, nad_Stack *other) {
    ASSERT_STACK(self);
    ASSERT_STACK(other);
    assert(nad_vec_elem_size(self->vec) == nad_vec_elem_size(other->vec));

    // self assignment is left to the vec, which already returns early on it: a guard
    // repeated here would be a branch no test could tell from its absence
    return nad_vec_copy_assign(self->vec, other->vec);
}

nad_Status nad_stack_move_assign(nad_Stack *self, nad_Stack *other) {
    ASSERT_STACK(self);
    ASSERT_STACK(other);
    assert(nad_vec_elem_size(self->vec) == nad_vec_elem_size(other->vec));

    // as in copy_assign, moving a stack onto itself is the vec's early return
    return nad_vec_move_assign(self->vec, other->vec);
}

/* ========== compare ========== */

bool nad_stack_eq(const nad_Stack *a, const nad_Stack *b) {
    ASSERT_STACK(a);
    ASSERT_STACK(b);

    return nad_vec_eq(a->vec, b->vec);
}

bool nad_stack_eq_by(const nad_Stack *a, const nad_Stack *b, nad_Eq eq) {
    ASSERT_STACK(a);
    ASSERT_STACK(b);

    return nad_vec_eq_by(a->vec, b->vec, eq);
}

/* ========== info ========== */

size_t nad_stack_len(const nad_Stack *self) {
    ASSERT_STACK(self);

    return nad_vec_len(self->vec);
}

size_t nad_stack_cap(const nad_Stack *self) {
    ASSERT_STACK(self);

    return nad_vec_cap(self->vec);
}

size_t nad_stack_elem_size(const nad_Stack *self) {
    ASSERT_STACK(self);

    return nad_vec_elem_size(self->vec);
}

nad_Al *nad_stack_al(const nad_Stack *self) {
    ASSERT_STACK(self);

    return nad_vec_al(self->vec);
}

/* ========== access ========== */

const void *nad_stack_top(const nad_Stack *self) {
    ASSERT_STACK(self);
    assert(nad_vec_len(self->vec) > 0);

    return nad_vec_last(self->vec);
}

void *nad_stack_top_mut(nad_Stack *self) {
    ASSERT_STACK(self);
    assert(nad_vec_len(self->vec) > 0);

    return nad_vec_last_mut(self->vec);
}

/* ========== mods ========== */

nad_Status nad_stack_push(nad_Stack *self, const void *val) {
    ASSERT_STACK(self);
    assert(val);

    return nad_vec_push(self->vec, val);
}

void nad_stack_pop(nad_Stack *self) {
    ASSERT_STACK(self);
    assert(nad_vec_len(self->vec) > 0);

    nad_vec_pop(self->vec);
}

void nad_stack_clear(nad_Stack *self) {
    ASSERT_STACK(self);

    nad_vec_clear(self->vec);
}

nad_Status nad_stack_reserve(nad_Stack *self, size_t new_cap) {
    ASSERT_STACK(self);

    return nad_vec_reserve(self->vec, new_cap);
}

nad_Status nad_stack_shrink_to_fit(nad_Stack *self) {
    ASSERT_STACK(self);

    return nad_vec_shrink_to_fit(self->vec);
}

nad_Status nad_stack_swap(nad_Stack *self, nad_Stack *other) {
    ASSERT_STACK(self);
    ASSERT_STACK(other);
    assert(nad_vec_elem_size(self->vec) == nad_vec_elem_size(other->vec));

    // as in copy_assign, swapping a stack with itself is the vec's early return
    return nad_vec_swap(self->vec, other->vec);
}

/* ========== to span ========== */

nad_Span nad_stack_to_span(const nad_Stack *self) {
    ASSERT_STACK(self);

    return nad_vec_to_span(self->vec);
}

/* ========== print ========== */

void nad_stack_fprint(const nad_Stack *self, FILE *stream, nad_FPrint fprint) {
    ASSERT_STACK(self);

    nad_vec_fprint(self->vec, stream, fprint);
}

void nad_stack_print(const nad_Stack *self, nad_FPrint fprint) {
    ASSERT_STACK(self);

    nad_vec_print(self->vec, fprint);
}

/* ========== internals ========== */

static nad_Status wrap(nad_Vec *vec, nad_Stack **out) {
    assert(vec);
    assert(out);

    nad_Stack *obj = nad_alloc(nad_vec_al(vec), sizeof(nad_Stack));
    if (!obj) {
        nad_vec_drop(vec);
        return NAD_STATUS_ERR_NO_MEM;
    }

    obj->vec = vec;

    *out = obj;

    return NAD_STATUS_OK;
}
