#include "nad/ds/arr.h"

#include "nad/algo/compare.h"
#include "nad/core/util.h"
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

struct nad_Arr {
    void *data;
    size_t len;
    size_t elem_size;
    nad_Al *al;
};

[[nodiscard]]
static nad_Status new_impl(bool zeroed, size_t len, size_t elem_size, nad_Al *al, nad_Arr **out);

static void set_fields(nad_Arr *obj, void *data, size_t len, size_t elem_size, nad_Al *al);

/// hands the block back and leaves an empty arr on the same allocator
static void release_data(nad_Arr *self);

[[nodiscard]]
static size_t len_bytes(const nad_Arr *self);

[[nodiscard]]
static const unsigned char *arr_offset(const nad_Arr *self, size_t idx);

[[nodiscard]]
static unsigned char *arr_offset_mut(nad_Arr *self, size_t idx);

/* ========== lifetime ========== */

nad_Status nad_arr_new_len(size_t len, size_t elem_size, nad_Al *al, nad_Arr **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    return new_impl(true, len, elem_size, al, out);
}

nad_Status nad_arr_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Arr **out) {
    assert(data || len == 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Arr *arr;
    const nad_Status st = new_impl(false, len, elem_size, al, &arr);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    if (len > 0) {
        memcpy(arr->data, data, len_bytes(arr));
    }

    *out = arr;

    return NAD_STATUS_OK;
}

nad_Status nad_arr_from_span(nad_Span s, nad_Al *al, nad_Arr **out) {
    NAD_SPAN_ASSERT(s);
    assert(al);
    assert(out);

    return nad_arr_from_data(s.data, s.len, s.elem_size, al, out);
}

void nad_arr_drop(nad_Arr *self) {
    if (!self) {
        return;
    }

    ASSERT_ARR(self);

    nad_Al *al_copy = self->al;
    nad_dealloc(al_copy, self->data, len_bytes(self));
    nad_dealloc(al_copy, self, sizeof(nad_Arr));
}

/* ========== copy ========== */

nad_Status nad_arr_copy(const nad_Arr *self, nad_Arr **out) {
    ASSERT_ARR(self);

    return nad_arr_copy_with(self, self->al, out);
}

nad_Status nad_arr_copy_with(const nad_Arr *self, nad_Al *al, nad_Arr **out) {
    ASSERT_ARR(self);
    assert(al);

    return nad_arr_from_span(nad_arr_to_span(self), al, out);
}

nad_Status nad_arr_copy_assign(const nad_Arr *self, nad_Arr *other) {
    ASSERT_ARR(self);
    ASSERT_ARR(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    const size_t self_bytes = len_bytes(self);
    const size_t other_bytes = len_bytes(other);

    if (self_bytes != other_bytes) {
        void *new_data = nad_realloc(other->al, other->data, other_bytes, self_bytes);
        if (self_bytes > 0 && !new_data) {
            return NAD_STATUS_ERR_NO_MEM;
        }

        other->data = new_data;
        other->len = self->len;
    }

    if (self_bytes > 0) {
        memcpy(other->data, self->data, self_bytes);
    }

    ASSERT_ARR(other);

    return NAD_STATUS_OK;
}

nad_Status nad_arr_move_assign(nad_Arr *self, nad_Arr *other) {
    ASSERT_ARR(self);
    ASSERT_ARR(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    // one allocator: the block is handed over as it is. What 'other' held ends up in 'self' and is released
    // there, through the very allocator that made it
    if (self->al == other->al) {
        NAD_SWAP(*self, *other);
        release_data(self);

        ASSERT_ARR(self);
        ASSERT_ARR(other);

        return NAD_STATUS_OK;
    }

    // two allocators: the whole copy is built on the target's before anything of it is
    // touched, so a refusal leaves both as they were
    nad_Arr *obj;
    const nad_Status st = nad_arr_copy_with(self, other->al, &obj);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    NAD_SWAP(*other, *obj);
    nad_arr_drop(obj);
    release_data(self);

    ASSERT_ARR(self);
    ASSERT_ARR(other);

    return NAD_STATUS_OK;
}

/* ========== compare ========== */

bool nad_arr_eq(const nad_Arr *a, const nad_Arr *b) {
    ASSERT_ARR(a);
    ASSERT_ARR(b);

    return nad_span_eq(nad_arr_to_span(a), nad_arr_to_span(b));
}

bool nad_arr_eq_by(const nad_Arr *a, const nad_Arr *b, nad_Eq eq) {
    ASSERT_ARR(a);
    ASSERT_ARR(b);
    assert(eq);

    return nad_span_eq_by(nad_arr_to_span(a), nad_arr_to_span(b), eq);
}

/* ========== info ========== */

size_t nad_arr_len(const nad_Arr *self) {
    ASSERT_ARR(self);

    return self->len;
}

size_t nad_arr_elem_size(const nad_Arr *self) {
    ASSERT_ARR(self);

    return self->elem_size;
}

size_t nad_arr_bytes(const nad_Arr *self) {
    ASSERT_ARR(self);

    return len_bytes(self);
}

nad_Al *nad_arr_al(const nad_Arr *self) {
    ASSERT_ARR(self);

    return self->al;
}

/* ========== access ========== */

const void *nad_arr_first(const nad_Arr *self) {
    ASSERT_ARR(self);
    assert(self->len > 0);

    return arr_offset(self, 0);
}

void *nad_arr_first_mut(nad_Arr *self) {
    ASSERT_ARR(self);
    assert(self->len > 0);

    return arr_offset_mut(self, 0);
}

const void *nad_arr_last(const nad_Arr *self) {
    ASSERT_ARR(self);
    assert(self->len > 0);

    return arr_offset(self, self->len - 1);
}

void *nad_arr_last_mut(nad_Arr *self) {
    ASSERT_ARR(self);
    assert(self->len > 0);

    return arr_offset_mut(self, self->len - 1);
}

const void *nad_arr_get(const nad_Arr *self, size_t idx) {
    ASSERT_ARR(self);
    assert(idx < self->len);

    return arr_offset(self, idx);
}

void *nad_arr_get_mut(nad_Arr *self, size_t idx) {
    ASSERT_ARR(self);
    assert(idx < self->len);

    return arr_offset_mut(self, idx);
}

void nad_arr_set(nad_Arr *self, size_t idx, const void *val) {
    ASSERT_ARR(self);
    assert(val);
    assert(idx < self->len);

    memcpy(arr_offset_mut(self, idx), val, self->elem_size);
}

const void *nad_arr_data(const nad_Arr *self) {
    ASSERT_ARR(self);

    return self->data;
}

void *nad_arr_data_mut(nad_Arr *self) {
    ASSERT_ARR(self);

    return self->data;
}

/* ========== mods ========== */

nad_Status nad_arr_swap(nad_Arr *self, nad_Arr *other) {
    ASSERT_ARR(self);
    ASSERT_ARR(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    if (self->al == other->al) {
        NAD_SWAP(*self, *other);
        return NAD_STATUS_OK;
    }

    const size_t self_bytes = len_bytes(self);
    const size_t other_bytes = len_bytes(other);

    void *self_new = nullptr;
    void *other_new = nullptr;

    if (other_bytes > 0) {
        self_new = nad_alloc(self->al, other_bytes);
        if (!self_new) {
            return NAD_STATUS_ERR_NO_MEM;
        }
    }

    if (self_bytes > 0) {
        other_new = nad_alloc(other->al, self_bytes);
        if (!other_new) {
            nad_dealloc(self->al, self_new, other_bytes);
            return NAD_STATUS_ERR_NO_MEM;
        }
    }

    if (other_bytes > 0) {
        memcpy(self_new, other->data, other_bytes);
    }

    if (self_bytes > 0) {
        memcpy(other_new, self->data, self_bytes);
    }

    nad_dealloc(self->al, self->data, self_bytes);
    nad_dealloc(other->al, other->data, other_bytes);

    self->data = self_new;
    other->data = other_new;
    NAD_SWAP(self->len, other->len);

    ASSERT_ARR(self);
    ASSERT_ARR(other);

    return NAD_STATUS_OK;
}

void nad_arr_swap_elems(nad_Arr *self, size_t i, size_t j) {
    ASSERT_ARR(self);

    nad_span_swap_elems(nad_arr_to_span_mut(self), i, j);
}

/* ========== to span ========== */

nad_SpanMut nad_arr_to_span_mut(nad_Arr *self) {
    ASSERT_ARR(self);

    return nad_span_new_mut(self->data, self->len, self->elem_size);
}

nad_Span nad_arr_to_span(const nad_Arr *self) {
    ASSERT_ARR(self);

    return nad_span_new(self->data, self->len, self->elem_size);
}

/* ========== print ========== */

void nad_arr_fprint(const nad_Arr *self, FILE *stream, nad_FPrint fprint) {
    ASSERT_ARR(self);
    assert(stream);
    assert(fprint);

    nad_span_fprint(nad_arr_to_span(self), stream, fprint);
}

void nad_arr_print(const nad_Arr *self, nad_FPrint fprint) {
    ASSERT_ARR(self);
    assert(fprint);

    nad_arr_fprint(self, stdout, fprint);
}

/* ========== internals ========== */

static nad_Status new_impl(bool zeroed, size_t len, size_t elem_size, nad_Al *al, nad_Arr **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Arr *obj = nad_alloc(al, sizeof(nad_Arr));
    if (!obj) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    void *data = nullptr;

    if (len > 0) {
        size_t bytes;
        if (ckd_mul(&bytes, len, elem_size)) {
            goto fail;
        }
        data = zeroed ? nad_calloc(al, len, elem_size) : nad_alloc(al, bytes);
        if (!data) {
            goto fail;
        }
    }

    set_fields(obj, data, len, elem_size, al);

    ASSERT_ARR(obj);

    *out = obj;
    return NAD_STATUS_OK;

fail:
    nad_dealloc(al, obj, sizeof(nad_Arr));
    return NAD_STATUS_ERR_NO_MEM;
}

static void set_fields(nad_Arr *obj, void *data, size_t len, size_t elem_size, nad_Al *al) {
    obj->data = data;
    obj->len = len;
    obj->elem_size = elem_size;
    obj->al = al;
}

static void release_data(nad_Arr *self) {
    nad_dealloc(self->al, self->data, len_bytes(self));
    self->data = nullptr;
    self->len = 0;
}

static size_t len_bytes(const nad_Arr *self) {
    return self->len * self->elem_size;
}

static const unsigned char *arr_offset(const nad_Arr *self, size_t idx) {
    return nad_byte_offset(self->data, self->elem_size, idx);
}

static unsigned char *arr_offset_mut(nad_Arr *self, size_t idx) {
    return nad_byte_offset_mut(self->data, self->elem_size, idx);
}
