#include "trs/ds/arr.h"

#include "trs/algo/compare.h"
#include "trs/core/util.h"

#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

/* ========== internals ========== */

#define ASSERT_ARR(a)                    \
    (assert(a),                          \
     assert((a)->elem_size > 0),         \
     assert((a)->len == 0 || (a)->data), \
     assert(!(a)->data || (a)->len > 0), \
     assert((a)->al))

struct trs_Arr {
    void *data;
    size_t len;
    size_t elem_size;
    trs_Al *al;
};

[[nodiscard]]
static trs_Status new_impl(bool zeroed, size_t len, size_t elem_size, trs_Al *al, trs_Arr **out);

static void set_fields(trs_Arr *obj, void *data, size_t len, size_t elem_size, trs_Al *al);

/// hands the block back and leaves an empty arr on the same allocator
static void release_data(trs_Arr *self);

[[nodiscard]]
static size_t len_bytes(const trs_Arr *self);

[[nodiscard]]
static const unsigned char *arr_offset(const trs_Arr *self, size_t idx);

[[nodiscard]]
static unsigned char *arr_offset_mut(trs_Arr *self, size_t idx);

/* ========== lifetime ========== */

trs_Status trs_arr_new_len(size_t len, size_t elem_size, trs_Al *al, trs_Arr **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(true, len, elem_size, al, out);
}

trs_Status trs_arr_from_data(const void *data, size_t len, size_t elem_size, trs_Al *al, trs_Arr **out) {
    assert(data || len == 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Arr *arr;
    const trs_Status st = new_impl(false, len, elem_size, al, &arr);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    if (len > 0) {
        memcpy(arr->data, data, len_bytes(arr));
    }

    *out = arr;

    return TRS_STATUS_OK;
}

trs_Status trs_arr_from_span(trs_Span s, trs_Al *al, trs_Arr **out) {
    TRS_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return trs_arr_from_data(s.data, s.len, s.elem_size, al, out);
}

void trs_arr_drop(trs_Arr *self) {
    if (!self) {
        return;
    }

    ASSERT_ARR(self);

    trs_Al *al_copy = self->al;
    trs_dealloc(al_copy, self->data, len_bytes(self));
    trs_dealloc(al_copy, self, sizeof(trs_Arr));
}

/* ========== copy ========== */

trs_Status trs_arr_copy(const trs_Arr *self, trs_Arr **out) {
    ASSERT_ARR(self);

    return trs_arr_copy_with(self, self->al, out);
}

trs_Status trs_arr_copy_with(const trs_Arr *self, trs_Al *al, trs_Arr **out) {
    ASSERT_ARR(self);
    assert(al);

    return trs_arr_from_span(trs_arr_to_span(self), al, out);
}

trs_Status trs_arr_copy_assign(const trs_Arr *self, trs_Arr *other) {
    ASSERT_ARR(self);
    ASSERT_ARR(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    const size_t self_bytes = len_bytes(self);
    const size_t other_bytes = len_bytes(other);

    if (self_bytes != other_bytes) {
        void *new_data = trs_realloc(other->al, other->data, other_bytes, self_bytes);
        if (self_bytes > 0 && !new_data) {
            return TRS_STATUS_ERR_NO_MEM;
        }

        other->data = new_data;
        other->len = self->len;
    }

    if (self_bytes > 0) {
        memcpy(other->data, self->data, self_bytes);
    }

    ASSERT_ARR(other);

    return TRS_STATUS_OK;
}

trs_Status trs_arr_move_assign(trs_Arr *self, trs_Arr *other) {
    ASSERT_ARR(self);
    ASSERT_ARR(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    // one allocator: the block is handed over as it is. What 'other' held ends up in 'self' and is released
    // there, through the very allocator that made it
    if (self->al == other->al) {
        TRS_SWAP(*self, *other);
        release_data(self);

        ASSERT_ARR(self);
        ASSERT_ARR(other);

        return TRS_STATUS_OK;
    }

    // two allocators: the whole copy is built on the target's before anything of it is
    // touched, so a refusal leaves both as they were
    trs_Arr *obj;
    const trs_Status st = trs_arr_copy_with(self, other->al, &obj);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    TRS_SWAP(*other, *obj);
    trs_arr_drop(obj);
    release_data(self);

    ASSERT_ARR(self);
    ASSERT_ARR(other);

    return TRS_STATUS_OK;
}

/* ========== compare ========== */

bool trs_arr_eq(const trs_Arr *a, const trs_Arr *b) {
    ASSERT_ARR(a);
    ASSERT_ARR(b);

    return trs_span_eq(trs_arr_to_span(a), trs_arr_to_span(b));
}

bool trs_arr_eq_by(const trs_Arr *a, const trs_Arr *b, trs_Eq eq) {
    ASSERT_ARR(a);
    ASSERT_ARR(b);
    assert(eq);

    return trs_span_eq_by(trs_arr_to_span(a), trs_arr_to_span(b), eq);
}

/* ========== info ========== */

size_t trs_arr_len(const trs_Arr *self) {
    ASSERT_ARR(self);

    return self->len;
}

size_t trs_arr_elem_size(const trs_Arr *self) {
    ASSERT_ARR(self);

    return self->elem_size;
}

size_t trs_arr_bytes(const trs_Arr *self) {
    ASSERT_ARR(self);

    return len_bytes(self);
}

trs_Al *trs_arr_al(const trs_Arr *self) {
    ASSERT_ARR(self);

    return self->al;
}

/* ========== access ========== */

const void *trs_arr_front(const trs_Arr *self) {
    ASSERT_ARR(self);
    assert(self->len > 0);

    return arr_offset(self, 0);
}

void *trs_arr_front_mut(trs_Arr *self) {
    ASSERT_ARR(self);
    assert(self->len > 0);

    return arr_offset_mut(self, 0);
}

const void *trs_arr_back(const trs_Arr *self) {
    ASSERT_ARR(self);
    assert(self->len > 0);

    return arr_offset(self, self->len - 1);
}

void *trs_arr_back_mut(trs_Arr *self) {
    ASSERT_ARR(self);
    assert(self->len > 0);

    return arr_offset_mut(self, self->len - 1);
}

const void *trs_arr_get(const trs_Arr *self, size_t idx) {
    ASSERT_ARR(self);
    assert(idx < self->len);

    return arr_offset(self, idx);
}

void *trs_arr_get_mut(trs_Arr *self, size_t idx) {
    ASSERT_ARR(self);
    assert(idx < self->len);

    return arr_offset_mut(self, idx);
}

void trs_arr_set(trs_Arr *self, size_t idx, const void *val) {
    ASSERT_ARR(self);
    assert(val);
    assert(idx < self->len);

    memcpy(arr_offset_mut(self, idx), val, self->elem_size);
}

const void *trs_arr_data(const trs_Arr *self) {
    ASSERT_ARR(self);

    return self->data;
}

void *trs_arr_data_mut(trs_Arr *self) {
    ASSERT_ARR(self);

    return self->data;
}

/* ========== mods ========== */

void trs_arr_swap(trs_Arr *self, trs_Arr *other) {
    ASSERT_ARR(self);
    ASSERT_ARR(other);
    assert(self->elem_size == other->elem_size);
    assert(self->al == other->al);

    if (self == other) {
        return;
    }

    TRS_SWAP(*self, *other);

    ASSERT_ARR(self);
    ASSERT_ARR(other);
}

void trs_arr_swap_elems(trs_Arr *self, size_t i, size_t j) {
    ASSERT_ARR(self);

    trs_span_swap_elems(trs_arr_to_span_mut(self), i, j);
}

/* ========== to span ========== */

trs_SpanMut trs_arr_to_span_mut(trs_Arr *self) {
    ASSERT_ARR(self);

    return trs_span_from_data_mut(self->data, self->len, self->elem_size);
}

trs_Span trs_arr_to_span(const trs_Arr *self) {
    ASSERT_ARR(self);

    return trs_span_from_data(self->data, self->len, self->elem_size);
}

/* ========== print ========== */

void trs_arr_fprint(const trs_Arr *self, FILE *stream, trs_FPrint fprint) {
    ASSERT_ARR(self);
    assert(stream);
    assert(fprint);

    trs_span_fprint(trs_arr_to_span(self), stream, fprint);
}

void trs_arr_print(const trs_Arr *self, trs_FPrint fprint) {
    ASSERT_ARR(self);
    assert(fprint);

    trs_arr_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

static trs_Status new_impl(bool zeroed, size_t len, size_t elem_size, trs_Al *al, trs_Arr **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    trs_Arr *obj = trs_alloc(al, sizeof(trs_Arr));
    if (!obj) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    void *data = nullptr;

    if (len > 0) {
        size_t bytes;
        if (ckd_mul(&bytes, len, elem_size)) {
            goto fail;
        }
        data = zeroed ? trs_calloc(al, len, elem_size) : trs_alloc(al, bytes);
        if (!data) {
            goto fail;
        }
    }

    set_fields(obj, data, len, elem_size, al);

    ASSERT_ARR(obj);

    *out = obj;
    return TRS_STATUS_OK;

fail:
    trs_dealloc(al, obj, sizeof(trs_Arr));
    return TRS_STATUS_ERR_NO_MEM;
}

static void set_fields(trs_Arr *obj, void *data, size_t len, size_t elem_size, trs_Al *al) {
    obj->data = data;
    obj->len = len;
    obj->elem_size = elem_size;
    obj->al = al;
}

static void release_data(trs_Arr *self) {
    trs_dealloc(self->al, self->data, len_bytes(self));
    self->data = nullptr;
    self->len = 0;
}

static size_t len_bytes(const trs_Arr *self) {
    return self->len * self->elem_size;
}

static const unsigned char *arr_offset(const trs_Arr *self, size_t idx) {
    return trs_byte_offset(self->data, self->elem_size, idx);
}

static unsigned char *arr_offset_mut(trs_Arr *self, size_t idx) {
    return trs_byte_offset_mut(self->data, self->elem_size, idx);
}
