#include "nad/ds/vec.h"
#include "internal/ptr.h"
#include "internal/todo.h"

#include <stdckdint.h>
#include <string.h>
#include <assert.h>

/* ========== internals ========== */

#define ASSERT_VEC(v)                    \
    (assert(v),                          \
     assert((v)->elem_size > 0),         \
     assert((v)->len <= (v)->cap),       \
     assert((v)->cap == 0 || (v)->data), \
     assert(!(v)->data || (v)->cap > 0), \
     assert((v)->al))

static constexpr size_t VEC_GROWTH_BASE = 1;
static constexpr size_t VEC_GROWTH_FACTOR = 2;

struct nad_Vec {
    void *data;
    size_t len;
    size_t cap;
    size_t elem_size;
    nad_Al *al;
};

[[nodiscard]] static
nad_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, nad_Al *al, nad_Vec **out);

static
void set_fields(nad_Vec *self, void *data, size_t len, size_t cap, size_t elem_size, nad_Al *al);

[[nodiscard]] static
size_t next_cap(const nad_Vec *self);

[[nodiscard]] static
nad_Status grow(nad_Vec *self);

[[nodiscard]] static
size_t len_bytes(const nad_Vec *self);

[[nodiscard]] static
size_t cap_bytes(const nad_Vec *self);

[[nodiscard]] static
const char *vec_offset(const nad_Vec *self, size_t idx);

[[nodiscard]] static
char *vec_offset_mut(nad_Vec *self, size_t idx);

/* ========== lifetime ========== */

nad_Status nad_vec_new(size_t elem_size, nad_Al *al, nad_Vec **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(false, 0, 0, elem_size, al, out);
}

nad_Status nad_vec_new_len(size_t len, size_t elem_size, nad_Al *al, nad_Vec **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(true, len, len, elem_size, al, out);
}

nad_Status nad_vec_new_cap(size_t cap, size_t elem_size, nad_Al *al, nad_Vec **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(false, 0, cap, elem_size, al, out);
}

nad_Status nad_vec_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Vec **out) {
    assert(data || len == 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Vec *vec;
    const nad_Status st = new_impl(false, len, len, elem_size, al, &vec);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    if (len > 0) {
        memcpy(vec->data, data, len_bytes(vec));
    }

    *out = vec;

    return NAD_STATUS_OK;
}

nad_Status nad_vec_from_span(nad_Span s, nad_Al *al, nad_Vec **out) {
    NAD_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return nad_vec_from_data(s.data, s.len, s.elem_size, al, out);
}

void nad_vec_drop(nad_Vec *self) {
    if (!self) {
        return;
    }

    ASSERT_VEC(self);

    nad_Al *al_copy = self->al;
    nad_dealloc(al_copy, self->data, cap_bytes(self));
    nad_dealloc(al_copy, self, sizeof(nad_Vec));
}

/* ========== copy ========== */

nad_Status nad_vec_copy(const nad_Vec *self, nad_Vec **out) {
    ASSERT_VEC(self);

    return nad_vec_from_span(nad_vec_to_span(self), self->al, out);
}

nad_Status nad_vec_copy_assign(const nad_Vec *self, nad_Vec *other) {
    ASSERT_VEC(self);
    ASSERT_VEC(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    const nad_Status st = nad_vec_reserve(other, self->len);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    if (self->len > 0) {
        memcpy(other->data, self->data, len_bytes(self));
    }

    other->len = self->len;

    ASSERT_VEC(other);

    return NAD_STATUS_OK;
}

/* ========== info ========== */

size_t nad_vec_len(const nad_Vec *self) {
    ASSERT_VEC(self);

    return self->len;
}

size_t nad_vec_cap(const nad_Vec *self) {
    ASSERT_VEC(self);

    return self->cap;
}

size_t nad_vec_elem_size(const nad_Vec *self) {
    ASSERT_VEC(self);

    return self->elem_size;
}

size_t nad_vec_bytes(const nad_Vec *self) {
    ASSERT_VEC(self);

    return len_bytes(self);
}

nad_Al *nad_vec_al(const nad_Vec *self) {
    ASSERT_VEC(self);

    return self->al;
}

/* ========== access ========== */

const void *nad_vec_first(const nad_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset(self, 0);
}

void *nad_vec_first_mut(nad_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset_mut(self, 0);
}

const void *nad_vec_last(const nad_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset(self, self->len - 1);
}

void *nad_vec_last_mut(nad_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset_mut(self, self->len - 1);
}

const void *nad_vec_get(const nad_Vec *self, size_t idx) {
    ASSERT_VEC(self);
    assert(idx < self->len);

    return vec_offset(self, idx);
}

void *nad_vec_get_mut(nad_Vec *self, size_t idx) {
    ASSERT_VEC(self);
    assert(idx < self->len);

    return vec_offset_mut(self, idx);
}

void nad_vec_set(nad_Vec *self, size_t idx, const void *val) {
    ASSERT_VEC(self);
    assert(val);
    assert(idx < self->len);

    memcpy(vec_offset_mut(self, idx), val, self->elem_size);
}

const void *nad_vec_data(const nad_Vec *self) {
    ASSERT_VEC(self);

    return self->data;
}

void *nad_vec_data_mut(nad_Vec *self) {
    ASSERT_VEC(self);

    return self->data;
}

/* ========== mods ========== */

nad_Status nad_vec_push(nad_Vec *self, const void *val) {
    ASSERT_VEC(self);
    assert(val);

    if (self->len == self->cap) {
        const nad_Status st = grow(self);
        if (NAD_STATUS_IS_ERR(st)) {
            return st;
        }
    }

    memcpy(vec_offset_mut(self, self->len), val, self->elem_size);
    ++self->len;

    return NAD_STATUS_OK;
}

void nad_vec_pop(nad_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    --self->len;
}

nad_Status nad_vec_insert(nad_Vec *self, size_t idx, const void *val) {
    ASSERT_VEC(self);
    assert(val);
    assert(idx <= self->len);

    if (self->len == self->cap) {
        const nad_Status st = grow(self);
        if (NAD_STATUS_IS_ERR(st)) {
            return st;
        }
    }

    const size_t tail = self->len - idx;
    if (tail > 0) {
        memmove(vec_offset_mut(self, idx + 1), vec_offset_mut(self, idx), tail * self->elem_size);
    }

    memcpy(vec_offset_mut(self, idx), val, self->elem_size);
    ++self->len;

    return NAD_STATUS_OK;
}

void nad_vec_remove(nad_Vec *self, size_t idx) {
    ASSERT_VEC(self);
    assert(idx < self->len);

    const size_t tail = self->len - idx - 1;
    if (tail > 0) {
        memmove(vec_offset_mut(self, idx), vec_offset_mut(self, idx + 1), tail * self->elem_size);
    }
    --self->len;
}

void nad_vec_clear(nad_Vec *self) {
    ASSERT_VEC(self);

    self->len = 0;
}

nad_Status nad_vec_reserve(nad_Vec *self, size_t new_cap) {
    ASSERT_VEC(self);

    if (new_cap <= self->cap) {
        return NAD_STATUS_OK;
    }

    size_t new_bytes;
    if (ckd_mul(&new_bytes, new_cap, self->elem_size)) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }
    void *data = nad_realloc(self->al, self->data, cap_bytes(self), new_bytes);
    if (!data) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    self->data = data;
    self->cap = new_cap;

    return NAD_STATUS_OK;
}

nad_Status nad_vec_shrink_to_fit(nad_Vec *self) {
    ASSERT_VEC(self);

    if (self->len == self->cap) {
        return NAD_STATUS_OK;
    }

    if (self->len == 0) {
        nad_dealloc(self->al, self->data, cap_bytes(self));
        self->data = nullptr;
        self->cap = 0;

        ASSERT_VEC(self);

        return NAD_STATUS_OK;
    }

    void *data = nad_realloc(self->al, self->data, cap_bytes(self), len_bytes(self));
    if (!data) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    self->data = data;
    self->cap = self->len;

    ASSERT_VEC(self);

    return NAD_STATUS_OK;
}

nad_Status nad_vec_resize(nad_Vec *self, size_t new_len) {
    ASSERT_VEC(self);

    if (new_len <= self->len) {
        self->len = new_len;
        return NAD_STATUS_OK;
    }

    if (new_len > self->cap) {
        const nad_Status st = nad_vec_reserve(self, new_len);
        if (NAD_STATUS_IS_ERR(st)) {
            return st;
        }
    }

    // zero init tail
    const size_t add_bytes = (new_len - self->len) * self->elem_size;
    memset(vec_offset_mut(self, self->len), 0, add_bytes);
    self->len = new_len;

    return NAD_STATUS_OK;
}

nad_Status nad_vec_swap(nad_Vec *self, nad_Vec *other) {
    NAD_NOT_IMPLEMENTED(self, other);
}

void nad_vec_swap_elems(nad_Vec *self, size_t i, size_t j) {
    ASSERT_VEC(self);

    nad_span_swap_elems(nad_vec_to_span_mut(self), i, j);
}

/* ========== to span ========== */

nad_SpanMut nad_vec_to_span_mut(nad_Vec *self) {
    ASSERT_VEC(self);

    return nad_span_new_mut(self->data, self->len, self->elem_size);
}

nad_Span nad_vec_to_span(const nad_Vec *self) {
    ASSERT_VEC(self);

    return nad_span_new(self->data, self->len, self->elem_size);
}

/* ========== internals ========== */

[[nodiscard]] static
nad_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, nad_Al *al, nad_Vec **out) {
    assert(len <= cap);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Vec *vec = nad_alloc(al, sizeof(nad_Vec));
    if (!vec) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    void *data = nullptr;

    if (cap > 0) {
        size_t bytes;
        if (ckd_mul(&bytes, cap, elem_size)) {
            goto fail;
        }
        data = nad_alloc(al, bytes);
        if (!data) {
            goto fail;
        }
        if (zeroed) {
            memset(data, 0, len * elem_size);
        }
    }

    set_fields(vec, data, len, cap, elem_size, al);

    ASSERT_VEC(vec);

    *out = vec;
    return NAD_STATUS_OK;

fail:
    nad_dealloc(al, vec, sizeof(nad_Vec));
    return NAD_STATUS_OUT_OF_MEMORY;
}

static
void set_fields(nad_Vec *self, void *data, size_t len, size_t cap, size_t elem_size, nad_Al *al) {
    self->data = data;
    self->len = len;
    self->cap = cap;
    self->elem_size = elem_size;
    self->al = al;
}

static
size_t next_cap(const nad_Vec *self) {
    if (self->cap == 0) {
        return VEC_GROWTH_BASE;
    }

    size_t grown;
    if (ckd_mul(&grown, self->cap, VEC_GROWTH_FACTOR)) {
        return SIZE_MAX;
    }

    return grown;
}

static
nad_Status grow(nad_Vec *self) {
    assert(self->len == self->cap);

    if (self->cap == SIZE_MAX) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    const size_t wanted = next_cap(self);

    const nad_Status st = nad_vec_reserve(self, wanted);
    if (NAD_STATUS_IS_OK(st) || wanted <= self->cap + 1) {
        return st;
    }

    return nad_vec_reserve(self, self->cap + 1);
}

static
size_t len_bytes(const nad_Vec *self) {
    return self->len * self->elem_size;
}

static
size_t cap_bytes(const nad_Vec *self) {
    return self->cap * self->elem_size;
}

static
const char *vec_offset(const nad_Vec *self, size_t idx) {
    return nad_byte_offset(self->data, self->elem_size, idx);
}

static
char *vec_offset_mut(nad_Vec *self, size_t idx) {
    return nad_byte_offset_mut(self->data, self->elem_size, idx);
}
