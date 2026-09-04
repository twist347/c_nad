#include "nad/ds/vec.h"

#include "nad/algo/compare.h"

#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

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

[[nodiscard]]
static nad_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, nad_Al *al, nad_Vec **out);

static void set_fields(nad_Vec *obj, void *data, size_t len, size_t cap, size_t elem_size, nad_Al *al);

/// hands the block back and leaves an empty vec on the same allocator
static void release_data(nad_Vec *self);

[[nodiscard]]
static size_t next_cap(const nad_Vec *self);

[[nodiscard]]
static nad_Status grow(nad_Vec *self);

/// room for one more elem, growing the block when it is full
[[nodiscard]]
static nad_Status reserve_one(nad_Vec *self);

/// room for 'new_len' elems, asked for with the growth factor when that is the bigger of
/// the two so a run of extends keeps the amortized cost a run of pushes has. Falls back
/// to the exact length when the eager request is refused
[[nodiscard]]
static nad_Status reserve_for(nad_Vec *self, size_t new_len);

[[nodiscard]]
static size_t len_bytes(const nad_Vec *self);

[[nodiscard]]
static size_t cap_bytes(const nad_Vec *self);

[[nodiscard]]
static const unsigned char *vec_offset(const nad_Vec *self, size_t idx);

[[nodiscard]]
static unsigned char *vec_offset_mut(nad_Vec *self, size_t idx);

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

    return nad_vec_copy_with(self, self->al, out);
}

nad_Status nad_vec_copy_with(const nad_Vec *self, nad_Al *al, nad_Vec **out) {
    ASSERT_VEC(self);
    assert(al);

    return nad_vec_from_span(nad_vec_to_span(self), al, out);
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

nad_Status nad_vec_move_assign(nad_Vec *self, nad_Vec *other) {
    ASSERT_VEC(self);
    ASSERT_VEC(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    // one allocator: the block is handed over, capacity and all. What 'other' held ends up in 'self' and is released
    // there, through the very allocator that made it
    if (self->al == other->al) {
        NAD_SWAP(*self, *other);
        release_data(self);

        ASSERT_VEC(self);
        ASSERT_VEC(other);

        return NAD_STATUS_OK;
    }

    // two allocators: the whole copy is built on the target's before anything of it is
    // touched, so a refusal leaves both as they were
    nad_Vec *obj;
    const nad_Status st = nad_vec_copy_with(self, other->al, &obj);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    NAD_SWAP(*other, *obj);
    nad_vec_drop(obj);
    release_data(self);

    ASSERT_VEC(self);
    ASSERT_VEC(other);

    return NAD_STATUS_OK;
}

/* ========== compare ========== */

bool nad_vec_eq(const nad_Vec *a, const nad_Vec *b) {
    ASSERT_VEC(a);
    ASSERT_VEC(b);

    return nad_span_eq(nad_vec_to_span(a), nad_vec_to_span(b));
}

bool nad_vec_eq_by(const nad_Vec *a, const nad_Vec *b, nad_Eq eq) {
    ASSERT_VEC(a);
    ASSERT_VEC(b);
    assert(eq);

    return nad_span_eq_by(nad_vec_to_span(a), nad_vec_to_span(b), eq);
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

const void *nad_vec_front(const nad_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset(self, 0);
}

void *nad_vec_front_mut(nad_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset_mut(self, 0);
}

const void *nad_vec_back(const nad_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset(self, self->len - 1);
}

void *nad_vec_back_mut(nad_Vec *self) {
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

    const nad_Status st = reserve_one(self);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
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

    const nad_Status st = reserve_one(self);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
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
        return NAD_STATUS_ERR_NO_MEM;
    }
    void *data = nad_realloc(self->al, self->data, cap_bytes(self), new_bytes);
    if (!data) {
        return NAD_STATUS_ERR_NO_MEM;
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
        return NAD_STATUS_ERR_NO_MEM;
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

void nad_vec_swap(nad_Vec *self, nad_Vec *other) {
    ASSERT_VEC(self);
    ASSERT_VEC(other);
    assert(self->elem_size == other->elem_size);
    assert(self->al == other->al);

    if (self == other) {
        return;
    }

    NAD_SWAP(*self, *other);

    ASSERT_VEC(self);
    ASSERT_VEC(other);
}

void nad_vec_swap_elems(nad_Vec *self, size_t i, size_t j) {
    ASSERT_VEC(self);

    nad_span_swap_elems(nad_vec_to_span_mut(self), i, j);
}

/* ========== bulk mods ========== */

nad_Status nad_vec_extend(nad_Vec *self, nad_Span src) {
    ASSERT_VEC(self);
    NAD_SPAN_ASSERT(src);
    assert(src.elem_size == self->elem_size);

    return nad_vec_insert_span(self, self->len, src);
}

nad_Status nad_vec_insert_span(nad_Vec *self, size_t idx, nad_Span src) {
    ASSERT_VEC(self);
    NAD_SPAN_ASSERT(src);
    assert(src.elem_size == self->elem_size);
    assert(idx <= self->len);

    if (src.len == 0) {
        return NAD_STATUS_OK;
    }

    size_t new_len;
    if (ckd_add(&new_len, self->len, src.len)) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    if (new_len > self->cap) {
        const nad_Status st = reserve_for(self, new_len);
        if (NAD_STATUS_IS_ERR(st)) {
            return st;
        }
    }

    // one move for the whole run: this is the difference from a loop of insert, which
    // walks the tail again for every elem
    const size_t tail = self->len - idx;
    if (tail > 0) {
        memmove(vec_offset_mut(self, idx + src.len), vec_offset_mut(self, idx), tail * self->elem_size);
    }

    memcpy(vec_offset_mut(self, idx), src.data, src.len * self->elem_size);
    self->len = new_len;

    return NAD_STATUS_OK;
}

void nad_vec_remove_range(nad_Vec *self, size_t idx, size_t count) {
    ASSERT_VEC(self);
    assert(idx <= self->len);
    assert(count <= self->len - idx);

    if (count == 0) {
        return;
    }

    const size_t tail = self->len - idx - count;
    if (tail > 0) {
        memmove(vec_offset_mut(self, idx), vec_offset_mut(self, idx + count), tail * self->elem_size);
    }

    self->len -= count;
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

/* ========== print ========== */

void nad_vec_fprint(const nad_Vec *self, FILE *stream, nad_FPrint fprint) {
    ASSERT_VEC(self);
    assert(stream);
    assert(fprint);

    nad_span_fprint(nad_vec_to_span(self), stream, fprint);
}

void nad_vec_print(const nad_Vec *self, nad_FPrint fprint) {
    ASSERT_VEC(self);
    assert(fprint);

    nad_vec_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

[[nodiscard]]
static nad_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, nad_Al *al, nad_Vec **out) {
    assert(len <= cap);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Vec *obj = nad_alloc(al, sizeof(nad_Vec));
    if (!obj) {
        return NAD_STATUS_ERR_NO_MEM;
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

    set_fields(obj, data, len, cap, elem_size, al);

    ASSERT_VEC(obj);

    *out = obj;
    return NAD_STATUS_OK;

fail:
    nad_dealloc(al, obj, sizeof(nad_Vec));
    return NAD_STATUS_ERR_NO_MEM;
}

static void set_fields(nad_Vec *obj, void *data, size_t len, size_t cap, size_t elem_size, nad_Al *al) {
    obj->data = data;
    obj->len = len;
    obj->cap = cap;
    obj->elem_size = elem_size;
    obj->al = al;
}

static size_t next_cap(const nad_Vec *self) {
    if (self->cap == 0) {
        return VEC_GROWTH_BASE;
    }

    size_t grown;
    if (ckd_mul(&grown, self->cap, VEC_GROWTH_FACTOR)) {
        return SIZE_MAX;
    }

    return grown;
}

static nad_Status grow(nad_Vec *self) {
    assert(self->len == self->cap);

    if (self->cap == SIZE_MAX) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    const size_t wanted = next_cap(self);

    const nad_Status st = nad_vec_reserve(self, wanted);
    if (NAD_STATUS_IS_OK(st) || wanted <= self->cap + 1) {
        return st;
    }

    return nad_vec_reserve(self, self->cap + 1);
}

static nad_Status reserve_one(nad_Vec *self) {
    return self->len == self->cap ? grow(self) : NAD_STATUS_OK;
}

static nad_Status reserve_for(nad_Vec *self, size_t new_len) {
    assert(new_len > self->cap);

    const size_t eager = next_cap(self);
    if (eager > new_len) {
        const nad_Status st = nad_vec_reserve(self, eager);
        if (NAD_STATUS_IS_OK(st)) {
            return st;
        }
    }

    return nad_vec_reserve(self, new_len);
}

static void release_data(nad_Vec *self) {
    nad_dealloc(self->al, self->data, cap_bytes(self));
    self->data = nullptr;
    self->len = 0;
    self->cap = 0;
}

static size_t len_bytes(const nad_Vec *self) {
    return self->len * self->elem_size;
}

static size_t cap_bytes(const nad_Vec *self) {
    return self->cap * self->elem_size;
}

static const unsigned char *vec_offset(const nad_Vec *self, size_t idx) {
    return nad_byte_offset(self->data, self->elem_size, idx);
}

static unsigned char *vec_offset_mut(nad_Vec *self, size_t idx) {
    return nad_byte_offset_mut(self->data, self->elem_size, idx);
}
