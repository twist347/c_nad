#include "trs/ds/vec.h"

#include "trs/algo/compare.h"

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

struct trs_Vec {
    void *data;
    size_t len;
    size_t cap;
    size_t elem_size;
    trs_Al *al;
};

[[nodiscard]]
static trs_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, trs_Al *al, trs_Vec **out);

static void set_fields(trs_Vec *obj, void *data, size_t len, size_t cap, size_t elem_size, trs_Al *al);

/// hands the block back and leaves an empty vec on the same allocator
static void release_data(trs_Vec *self);

[[nodiscard]]
static size_t next_cap(const trs_Vec *self);

[[nodiscard]]
static trs_Status grow(trs_Vec *self);

/// room for one more elem, growing the block when it is full
[[nodiscard]]
static trs_Status reserve_one(trs_Vec *self);

/// room for 'new_len' elems, asked for with the growth factor when that is the bigger of
/// the two so a run of extends keeps the amortized cost a run of pushes has. Falls back
/// to the exact length when the eager request is refused
[[nodiscard]]
static trs_Status reserve_for(trs_Vec *self, size_t new_len);

[[nodiscard]]
static size_t len_bytes(const trs_Vec *self);

[[nodiscard]]
static size_t cap_bytes(const trs_Vec *self);

[[nodiscard]]
static const unsigned char *vec_offset(const trs_Vec *self, size_t idx);

[[nodiscard]]
static unsigned char *vec_offset_mut(trs_Vec *self, size_t idx);

/* ========== lifetime ========== */

trs_Status trs_vec_new(size_t elem_size, trs_Al *al, trs_Vec **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(false, 0, 0, elem_size, al, out);
}

trs_Status trs_vec_new_len(size_t len, size_t elem_size, trs_Al *al, trs_Vec **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(true, len, len, elem_size, al, out);
}

trs_Status trs_vec_new_cap(size_t cap, size_t elem_size, trs_Al *al, trs_Vec **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(false, 0, cap, elem_size, al, out);
}

trs_Status trs_vec_from_data(const void *data, size_t len, size_t elem_size, trs_Al *al, trs_Vec **out) {
    assert(data || len == 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Vec *vec;
    const trs_Status st = new_impl(false, len, len, elem_size, al, &vec);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    if (len > 0) {
        memcpy(vec->data, data, len_bytes(vec));
    }

    *out = vec;

    return TRS_STATUS_OK;
}

trs_Status trs_vec_from_span(trs_Span s, trs_Al *al, trs_Vec **out) {
    TRS_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return trs_vec_from_data(s.data, s.len, s.elem_size, al, out);
}

void trs_vec_drop(trs_Vec *self) {
    if (!self) {
        return;
    }

    ASSERT_VEC(self);

    trs_Al *al_copy = self->al;
    trs_dealloc(al_copy, self->data, cap_bytes(self));
    trs_dealloc(al_copy, self, sizeof(trs_Vec));
}

/* ========== copy ========== */

trs_Status trs_vec_copy(const trs_Vec *self, trs_Vec **out) {
    ASSERT_VEC(self);

    return trs_vec_copy_with(self, self->al, out);
}

trs_Status trs_vec_copy_with(const trs_Vec *self, trs_Al *al, trs_Vec **out) {
    ASSERT_VEC(self);
    assert(al);

    return trs_vec_from_span(trs_vec_to_span(self), al, out);
}

trs_Status trs_vec_copy_assign(const trs_Vec *self, trs_Vec *other) {
    ASSERT_VEC(self);
    ASSERT_VEC(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    const trs_Status st = trs_vec_reserve(other, self->len);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    if (self->len > 0) {
        memcpy(other->data, self->data, len_bytes(self));
    }

    other->len = self->len;

    ASSERT_VEC(other);

    return TRS_STATUS_OK;
}

trs_Status trs_vec_move_assign(trs_Vec *self, trs_Vec *other) {
    ASSERT_VEC(self);
    ASSERT_VEC(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    // one allocator: the block is handed over, capacity and all. What 'other' held ends up in 'self' and is released
    // there, through the very allocator that made it
    if (self->al == other->al) {
        TRS_SWAP(*self, *other);
        release_data(self);

        ASSERT_VEC(self);
        ASSERT_VEC(other);

        return TRS_STATUS_OK;
    }

    // two allocators: the whole copy is built on the target's before anything of it is
    // touched, so a refusal leaves both as they were
    trs_Vec *obj;
    const trs_Status st = trs_vec_copy_with(self, other->al, &obj);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    TRS_SWAP(*other, *obj);
    trs_vec_drop(obj);
    release_data(self);

    ASSERT_VEC(self);
    ASSERT_VEC(other);

    return TRS_STATUS_OK;
}

/* ========== compare ========== */

bool trs_vec_eq(const trs_Vec *a, const trs_Vec *b) {
    ASSERT_VEC(a);
    ASSERT_VEC(b);

    return trs_span_eq(trs_vec_to_span(a), trs_vec_to_span(b));
}

bool trs_vec_eq_by(const trs_Vec *a, const trs_Vec *b, trs_Eq eq) {
    ASSERT_VEC(a);
    ASSERT_VEC(b);
    assert(eq);

    return trs_span_eq_by(trs_vec_to_span(a), trs_vec_to_span(b), eq);
}

/* ========== info ========== */

size_t trs_vec_len(const trs_Vec *self) {
    ASSERT_VEC(self);

    return self->len;
}

size_t trs_vec_cap(const trs_Vec *self) {
    ASSERT_VEC(self);

    return self->cap;
}

size_t trs_vec_elem_size(const trs_Vec *self) {
    ASSERT_VEC(self);

    return self->elem_size;
}

size_t trs_vec_bytes(const trs_Vec *self) {
    ASSERT_VEC(self);

    return len_bytes(self);
}

trs_Al *trs_vec_al(const trs_Vec *self) {
    ASSERT_VEC(self);

    return self->al;
}

/* ========== access ========== */

const void *trs_vec_front(const trs_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset(self, 0);
}

void *trs_vec_front_mut(trs_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset_mut(self, 0);
}

const void *trs_vec_back(const trs_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset(self, self->len - 1);
}

void *trs_vec_back_mut(trs_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    return vec_offset_mut(self, self->len - 1);
}

const void *trs_vec_get(const trs_Vec *self, size_t idx) {
    ASSERT_VEC(self);
    assert(idx < self->len);

    return vec_offset(self, idx);
}

void *trs_vec_get_mut(trs_Vec *self, size_t idx) {
    ASSERT_VEC(self);
    assert(idx < self->len);

    return vec_offset_mut(self, idx);
}

void trs_vec_set(trs_Vec *self, size_t idx, const void *val) {
    ASSERT_VEC(self);
    assert(val);
    assert(idx < self->len);

    memcpy(vec_offset_mut(self, idx), val, self->elem_size);
}

const void *trs_vec_data(const trs_Vec *self) {
    ASSERT_VEC(self);

    return self->data;
}

void *trs_vec_data_mut(trs_Vec *self) {
    ASSERT_VEC(self);

    return self->data;
}

/* ========== mods ========== */

trs_Status trs_vec_push(trs_Vec *self, const void *val) {
    ASSERT_VEC(self);
    assert(val);

    const trs_Status st = reserve_one(self);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    memcpy(vec_offset_mut(self, self->len), val, self->elem_size);
    ++self->len;

    return TRS_STATUS_OK;
}

void trs_vec_pop(trs_Vec *self) {
    ASSERT_VEC(self);
    assert(self->len > 0);

    --self->len;
}

trs_Status trs_vec_insert(trs_Vec *self, size_t idx, const void *val) {
    ASSERT_VEC(self);
    assert(val);
    assert(idx <= self->len);

    const trs_Status st = reserve_one(self);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    const size_t tail = self->len - idx;
    if (tail > 0) {
        memmove(vec_offset_mut(self, idx + 1), vec_offset_mut(self, idx), tail * self->elem_size);
    }

    memcpy(vec_offset_mut(self, idx), val, self->elem_size);
    ++self->len;

    return TRS_STATUS_OK;
}

void trs_vec_remove(trs_Vec *self, size_t idx) {
    ASSERT_VEC(self);
    assert(idx < self->len);

    const size_t tail = self->len - idx - 1;
    if (tail > 0) {
        memmove(vec_offset_mut(self, idx), vec_offset_mut(self, idx + 1), tail * self->elem_size);
    }
    --self->len;
}

void trs_vec_clear(trs_Vec *self) {
    ASSERT_VEC(self);

    self->len = 0;
}

trs_Status trs_vec_reserve(trs_Vec *self, size_t new_cap) {
    ASSERT_VEC(self);

    if (new_cap <= self->cap) {
        return TRS_STATUS_OK;
    }

    size_t new_bytes;
    if (ckd_mul(&new_bytes, new_cap, self->elem_size)) {
        return TRS_STATUS_ERR_NO_MEM;
    }
    void *data = trs_realloc(self->al, self->data, cap_bytes(self), new_bytes);
    if (!data) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    self->data = data;
    self->cap = new_cap;

    return TRS_STATUS_OK;
}

trs_Status trs_vec_shrink_to_fit(trs_Vec *self) {
    ASSERT_VEC(self);

    if (self->len == self->cap) {
        return TRS_STATUS_OK;
    }

    if (self->len == 0) {
        trs_dealloc(self->al, self->data, cap_bytes(self));
        self->data = nullptr;
        self->cap = 0;

        ASSERT_VEC(self);

        return TRS_STATUS_OK;
    }

    void *data = trs_realloc(self->al, self->data, cap_bytes(self), len_bytes(self));
    if (!data) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    self->data = data;
    self->cap = self->len;

    ASSERT_VEC(self);

    return TRS_STATUS_OK;
}

trs_Status trs_vec_resize(trs_Vec *self, size_t new_len) {
    ASSERT_VEC(self);

    if (new_len <= self->len) {
        self->len = new_len;
        return TRS_STATUS_OK;
    }

    if (new_len > self->cap) {
        const trs_Status st = trs_vec_reserve(self, new_len);
        if (TRS_STATUS_IS_ERR(st)) {
            return st;
        }
    }

    // zero init tail
    const size_t add_bytes = (new_len - self->len) * self->elem_size;
    memset(vec_offset_mut(self, self->len), 0, add_bytes);
    self->len = new_len;

    return TRS_STATUS_OK;
}

void trs_vec_swap(trs_Vec *self, trs_Vec *other) {
    ASSERT_VEC(self);
    ASSERT_VEC(other);
    assert(self->elem_size == other->elem_size);
    assert(self->al == other->al);

    if (self == other) {
        return;
    }

    TRS_SWAP(*self, *other);

    ASSERT_VEC(self);
    ASSERT_VEC(other);
}

void trs_vec_swap_elems(trs_Vec *self, size_t i, size_t j) {
    ASSERT_VEC(self);

    trs_span_swap_elems(trs_vec_to_span_mut(self), i, j);
}

/* ========== bulk mods ========== */

trs_Status trs_vec_extend(trs_Vec *self, trs_Span src) {
    ASSERT_VEC(self);
    TRS_SPAN_ASSERT(src);
    assert(src.elem_size == self->elem_size);

    return trs_vec_insert_span(self, self->len, src);
}

trs_Status trs_vec_insert_span(trs_Vec *self, size_t idx, trs_Span src) {
    ASSERT_VEC(self);
    TRS_SPAN_ASSERT(src);
    assert(src.elem_size == self->elem_size);
    assert(idx <= self->len);

    if (src.len == 0) {
        return TRS_STATUS_OK;
    }

    size_t new_len;
    if (ckd_add(&new_len, self->len, src.len)) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    if (new_len > self->cap) {
        const trs_Status st = reserve_for(self, new_len);
        if (TRS_STATUS_IS_ERR(st)) {
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

    return TRS_STATUS_OK;
}

void trs_vec_remove_range(trs_Vec *self, size_t idx, size_t count) {
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

trs_SpanMut trs_vec_to_span_mut(trs_Vec *self) {
    ASSERT_VEC(self);

    return trs_span_from_data_mut(self->data, self->len, self->elem_size);
}

trs_Span trs_vec_to_span(const trs_Vec *self) {
    ASSERT_VEC(self);

    return trs_span_from_data(self->data, self->len, self->elem_size);
}

/* ========== print ========== */

void trs_vec_fprint(const trs_Vec *self, FILE *stream, trs_FPrint fprint) {
    ASSERT_VEC(self);
    assert(stream);
    assert(fprint);

    trs_span_fprint(trs_vec_to_span(self), stream, fprint);
}

void trs_vec_print(const trs_Vec *self, trs_FPrint fprint) {
    ASSERT_VEC(self);
    assert(fprint);

    trs_vec_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

[[nodiscard]]
static trs_Status new_impl(bool zeroed, size_t len, size_t cap, size_t elem_size, trs_Al *al, trs_Vec **out) {
    assert(len <= cap);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Vec *obj = trs_alloc(al, sizeof(trs_Vec));
    if (!obj) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    void *data = nullptr;

    if (cap > 0) {
        size_t bytes;
        if (ckd_mul(&bytes, cap, elem_size)) {
            goto fail;
        }
        data = trs_alloc(al, bytes);
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
    return TRS_STATUS_OK;

fail:
    trs_dealloc(al, obj, sizeof(trs_Vec));
    return TRS_STATUS_ERR_NO_MEM;
}

static void set_fields(trs_Vec *obj, void *data, size_t len, size_t cap, size_t elem_size, trs_Al *al) {
    obj->data = data;
    obj->len = len;
    obj->cap = cap;
    obj->elem_size = elem_size;
    obj->al = al;
}

static size_t next_cap(const trs_Vec *self) {
    if (self->cap == 0) {
        return VEC_GROWTH_BASE;
    }

    size_t grown;
    if (ckd_mul(&grown, self->cap, VEC_GROWTH_FACTOR)) {
        return SIZE_MAX;
    }

    return grown;
}

static trs_Status grow(trs_Vec *self) {
    assert(self->len == self->cap);

    if (self->cap == SIZE_MAX) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    const size_t wanted = next_cap(self);

    const trs_Status st = trs_vec_reserve(self, wanted);
    if (TRS_STATUS_IS_OK(st) || wanted <= self->cap + 1) {
        return st;
    }

    return trs_vec_reserve(self, self->cap + 1);
}

static trs_Status reserve_one(trs_Vec *self) {
    return self->len == self->cap ? grow(self) : TRS_STATUS_OK;
}

static trs_Status reserve_for(trs_Vec *self, size_t new_len) {
    assert(new_len > self->cap);

    const size_t eager = next_cap(self);
    if (eager > new_len) {
        const trs_Status st = trs_vec_reserve(self, eager);
        if (TRS_STATUS_IS_OK(st)) {
            return st;
        }
    }

    return trs_vec_reserve(self, new_len);
}

static void release_data(trs_Vec *self) {
    trs_dealloc(self->al, self->data, cap_bytes(self));
    self->data = nullptr;
    self->len = 0;
    self->cap = 0;
}

static size_t len_bytes(const trs_Vec *self) {
    return self->len * self->elem_size;
}

static size_t cap_bytes(const trs_Vec *self) {
    return self->cap * self->elem_size;
}

static const unsigned char *vec_offset(const trs_Vec *self, size_t idx) {
    return trs_byte_offset(self->data, self->elem_size, idx);
}

static unsigned char *vec_offset_mut(trs_Vec *self, size_t idx) {
    return trs_byte_offset_mut(self->data, self->elem_size, idx);
}
