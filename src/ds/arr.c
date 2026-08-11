#include "nad/ds/arr.h"
#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

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

static void set_fields(nad_Arr *arr, void *data, size_t len, size_t elem_size, nad_Al *al);
static nad_Status data_new_copy(const void *src, size_t len, size_t elem_size, nad_Al *al, void **out);

/* ========== lifetime ========== */

nad_Status nad_arr_new(size_t len, size_t elem_size, nad_Al *al, nad_Arr **out) {
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Arr *arr = nad_alloc(al, sizeof(nad_Arr));
    if (!arr) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    set_fields(arr, nullptr, 0, elem_size, al);

    if (len > 0) {
        // nad_calloc checks len * elem_size for overflow (ckd_mul) and returns null,
        // so overflow folds into the same failure path as a real allocation failure.
        void *data = nad_calloc(al, len, elem_size);
        if (!data) {
            nad_dealloc(al, arr, sizeof(nad_Arr));
            return NAD_STATUS_OUT_OF_MEMORY;
        }
        set_fields(arr, data, len, elem_size, al);
    }

    ASSERT_ARR(arr);

    *out = arr;
    return NAD_STATUS_OK;
}

nad_Status nad_arr_from_data(const void *data, size_t len, size_t elem_size, nad_Al *al, nad_Arr **out) {
    assert(data || len == 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    nad_Arr *arr = nad_alloc(al, sizeof(nad_Arr));
    if (!arr) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    set_fields(arr, nullptr, 0, elem_size, al);

    if (len > 0) {
        void *copy;
        const nad_Status st = data_new_copy(data, len, elem_size, al, &copy);
        if (NAD_STATUS_IS_ERR(st)) {
            nad_dealloc(al, arr, sizeof(nad_Arr));
            return st;
        }
        set_fields(arr, copy, len, elem_size, al);
    }

    ASSERT_ARR(arr);

    *out = arr;

    return NAD_STATUS_OK;
}

void nad_arr_drop(nad_Arr *self) {
    if (!self) {
        return;
    }

    ASSERT_ARR(self);

    nad_Al *al_copy = self->al;
    nad_dealloc(al_copy, self->data, self->len * self->elem_size);
    nad_dealloc(al_copy, self, sizeof(nad_Arr));
}

/* ========== copy ========== */

nad_Status nad_arr_copy(const nad_Arr *self, nad_Arr **out) {
    ASSERT_ARR(self);

    return nad_arr_from_data(self->data, self->len, self->elem_size, self->al, out);
}

nad_Status nad_arr_copy_assign(const nad_Arr *self, nad_Arr *other) {
    ASSERT_ARR(self);
    ASSERT_ARR(other);
    assert(self->elem_size == other->elem_size);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    const size_t elem_size = self->elem_size;
    const size_t self_bytes = self->len * elem_size;
    const size_t other_bytes = other->len * elem_size;

    if (other_bytes == self_bytes) {
        if (self_bytes > 0) {
            memcpy(other->data, self->data, self_bytes);
        }
        return NAD_STATUS_OK;
    }

    if (self_bytes == 0) {
        nad_dealloc(other->al, other->data, other_bytes);
        other->data = nullptr;
        other->len = 0;

        ASSERT_ARR(other);

        return NAD_STATUS_OK;
    }

    void *new_data = nad_realloc(other->al, other->data, other_bytes, self_bytes);
    if (!new_data) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    memcpy(new_data, self->data, self_bytes);
    other->data = new_data;
    other->len = self->len;

    ASSERT_ARR(other);

    return NAD_STATUS_OK;
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

nad_Al *nad_arr_al(const nad_Arr *self) {
    ASSERT_ARR(self);

    return self->al;
}

/* ========== access ========== */

const void *nad_arr_get(const nad_Arr *self, size_t idx) {
    ASSERT_ARR(self);
    assert(idx < self->len);

    return nad_byte_offset(self->data, self->elem_size, idx);
}

void *nad_arr_get_mut(nad_Arr *self, size_t idx) {
    ASSERT_ARR(self);
    assert(idx < self->len);

    return nad_byte_offset_mut(self->data, self->elem_size, idx);
}

void nad_arr_set(nad_Arr *self, size_t idx, const void *val) {
    ASSERT_ARR(self);
    assert(val);
    assert(idx < self->len);

    memcpy(nad_byte_offset_mut(self->data, self->elem_size, idx), val, self->elem_size);
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

void nad_arr_swap(nad_Arr *self, nad_Arr *other) {
    ASSERT_ARR(self);
    ASSERT_ARR(other);

    if (self == other) {
        return;
    }

    const nad_Arr tmp = *self;
    *self = *other;
    *other = tmp;
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

static void set_fields(nad_Arr *arr, void *data, size_t len, size_t elem_size, nad_Al *alloc) {
    arr->data = data;
    arr->len = len;
    arr->elem_size = elem_size;
    arr->al = alloc;
}

static nad_Status data_new_copy(const void *src, size_t len, size_t elem_size, nad_Al *al, void **out) {
    assert(src);
    assert(len > 0);
    assert(elem_size > 0);
    assert(al);
    assert(out);

    size_t bytes;
    if (ckd_mul(&bytes, len, elem_size)) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    void *data = nad_alloc(al, bytes);
    if (!data) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    memcpy(data, src, bytes);

    *out = data;

    return NAD_STATUS_OK;
}