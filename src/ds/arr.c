#include "nad/ds/arr.h"

#include <assert.h>
#include <string.h>

#include "nad/mem/alloc_default.h"

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

    *out = arr;
    return NAD_STATUS_OK;
}

void nad_arr_drop(nad_Arr *self) {
    if (!self) {
        return;
    }

    nad_Allocator *alloc = self->alloc;
    nad_dealloc(alloc, self->data, self->len * self->elem_size);
    nad_dealloc(alloc, self, sizeof(nad_Arr));
}

/* ========== copy ========== */

[[nodiscard]] nad_Status nad_arr_copy(const nad_Arr *self, nad_Arr **out) {
    assert(self);
    assert(out);

    nad_Arr *copy;
    nad_Status st = nad_arr_new(self->len, self->elem_size, self->alloc, &copy);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    if (self->len > 0) {
        memcpy(copy->data, self->data, self->len * self->elem_size);
    }

    *out = copy;
    return NAD_STATUS_OK;
}

[[nodiscard]] nad_Status nad_arr_copy_into(const nad_Arr *self, nad_Arr *other) {
    assert(self);
    assert(other);
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
        return NAD_STATUS_OK;
    }

    void *new_data = nad_realloc(other->alloc, other->data, other_bytes, self_bytes);
    if (!new_data) {
        return NAD_STATUS_OUT_OF_MEMORY;
    }

    memcpy(new_data, self->data, self_bytes);
    other->data = new_data;
    other->len = self->len;
    return NAD_STATUS_OK;
}

/* ========== info ========== */

size_t nad_arr_len(const nad_Arr *self) {
    assert(self);
    return self->len;
}

size_t nad_arr_elem_size(const nad_Arr *self) {
    assert(self);
    return self->elem_size;
}

nad_Allocator *nad_arr_alloc(const nad_Arr *self) {
    assert(self);
    return self->alloc;
}

/* ========== access ========== */

const void *nad_arr_get(const nad_Arr *self, size_t idx) {
    assert(self);
    assert(idx < self->len);

    return byte_offset(self->data, self->elem_size, idx);
}

void *nad_arr_get_mut(nad_Arr *self, size_t idx) {
    assert(self);
    assert(idx < self->len);

    return byte_offset(self->data, self->elem_size, idx);
}

void nad_arr_set(nad_Arr *self, size_t idx, const void *val) {
    assert(self);
    assert(val);
    assert(idx < self->len);

    memcpy(byte_offset(self->data, self->elem_size, idx), val, self->elem_size);
}

const void *nad_arr_data(const nad_Arr *self) {
    assert(self);

    return self->data;
}

void *nad_arr_data_mut(nad_Arr *self) {
    assert(self);

    return self->data;
}

/* ========== mods ========== */

void nad_arr_fill(nad_Arr *self, const void *val) {
    assert(self);
    assert(val);

    for (size_t i = 0; i < self->len; ++i) {
        memcpy(byte_offset(self->data, self->elem_size, i), val, self->elem_size);
    }
}

void nad_arr_swap_elems(nad_Arr *self, size_t i, size_t j) {
    assert(self);
    assert(i < self->len);
    assert(j < self->len);

    if (i == j) {
        return;
    }

    char *a = byte_offset(self->data, self->elem_size, i);
    char *b = byte_offset(self->data, self->elem_size, j);
    for (size_t k = 0; k < self->elem_size; ++k) {
        const char tmp = a[k];
        a[k] = b[k];
        b[k] = tmp;
    }
}

void nad_arr_swap(nad_Arr *self, nad_Arr *other) {
    assert(self);
    assert(other);

    if (self == other) {
        return;
    }

    const nad_Arr tmp = *self;
    *self = *other;
    *other = tmp;
}

/* ========== rels ========== */

bool nad_arr_eq(const nad_Arr *self, const nad_Arr *other) {
    assert(self);
    assert(other);

    if (self == other) {
        return true;
    }
    if (self->elem_size != other->elem_size || self->len != other->len) {
        return false;
    }
    if (self->len == 0) {
        return true;
    }
    return memcmp(self->data, other->data, self->len * other->elem_size) == 0;
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
