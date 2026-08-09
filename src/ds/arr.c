#include "nad/ds/arr.h"

#include <assert.h>
#include <string.h>

#define ASSERT_ARR(a)                    \
    (assert(a),                          \
     assert((a)->elem_size > 0),         \
     assert((a)->len == 0 || (a)->data), \
     assert(!(a)->data || (a)->len > 0), \
     assert((a)->alloc))

struct nad_Arr {
    void *data;
    size_t len;
    size_t elem_size;
    nad_Allocator *alloc;
};

static void set_fields(nad_Arr *arr, void *data, size_t len, size_t elem_size, nad_Allocator *alloc);

static char *byte_offset(void *base, size_t stride, size_t n);

/* ========== lifetime ========== */

nad_Status nad_arr_new(size_t len, size_t elem_size, nad_Allocator *alloc, nad_Arr **out) {
    assert(elem_size > 0);
    assert(alloc);
    assert(out);

    nad_Arr *arr = nad_alloc(alloc, sizeof(nad_Arr));
    if (!arr) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    set_fields(arr, nullptr, 0, elem_size, alloc);

    if (len > 0) {
        // nad_calloc checks len * elem_size for overflow (ckd_mul) and returns null,
        // so overflow folds into the same failure path as a real allocation failure.
        void *data = nad_calloc(alloc, len, elem_size);
        if (!data) {
            nad_dealloc(alloc, arr, sizeof(nad_Arr));
            return NAD_STATUS_OUT_OF_MEMORY;
        }
        set_fields(arr, data, len, elem_size, alloc);
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

    nad_Allocator *alloc_copy = self->alloc;
    nad_dealloc(alloc_copy, self->data, self->len * self->elem_size);
    nad_dealloc(alloc_copy, self, sizeof(nad_Arr));
}

/* ========== copy ========== */

nad_Status nad_arr_copy(const nad_Arr *self, nad_Arr **out) {
    ASSERT_ARR(self);
    assert(out);

    nad_Arr *copy;
    const nad_Status st = nad_arr_new(self->len, self->elem_size, self->alloc, &copy);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    if (self->len > 0) {
        memcpy(copy->data, self->data, self->len * self->elem_size);
    }

    ASSERT_ARR(copy);

    *out = copy;
    return NAD_STATUS_OK;
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
        nad_dealloc(other->alloc, other->data, other_bytes);
        other->data = nullptr;
        other->len = 0;

        ASSERT_ARR(other);

        return NAD_STATUS_OK;
    }

    void *new_data = nad_realloc(other->alloc, other->data, other_bytes, self_bytes);
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

nad_Allocator *nad_arr_alloc(const nad_Arr *self) {
    ASSERT_ARR(self);

    return self->alloc;
}

/* ========== access ========== */

const void *nad_arr_get(const nad_Arr *self, size_t idx) {
    ASSERT_ARR(self);
    assert(idx < self->len);

    return byte_offset(self->data, self->elem_size, idx);
}

void *nad_arr_get_mut(nad_Arr *self, size_t idx) {
    ASSERT_ARR(self);
    assert(idx < self->len);

    return byte_offset(self->data, self->elem_size, idx);
}

void nad_arr_set(nad_Arr *self, size_t idx, const void *val) {
    ASSERT_ARR(self);
    assert(val);
    assert(idx < self->len);

    memcpy(byte_offset(self->data, self->elem_size, idx), val, self->elem_size);
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

static void set_fields(nad_Arr *arr, void *data, size_t len, size_t elem_size, nad_Allocator *alloc) {
    arr->data = data;
    arr->len = len;
    arr->elem_size = elem_size;
    arr->alloc = alloc;
}

static char *byte_offset(void *base, size_t stride, size_t n) {
    return (char *) base + stride * n;
}
