#include "nad/alloc/alloc.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

void *nad_alloc(nad_Al *al, size_t size) {
    assert(al);
    assert(al->alloc);

    if (size == 0) {
        return nullptr;
    }

    return al->alloc(al->ctx, size);
}

void *nad_calloc(nad_Al *al, size_t num, size_t size) {
    assert(al);

    if (num == 0 || size == 0) {
        return nullptr;
    }

    // fallback
    if (!al->calloc) {
        size_t total;
        if (ckd_mul(&total, num, size)) {
            return nullptr;
        }
        void *ptr = nad_alloc(al, total);
        if (!ptr) {
            return nullptr;
        }
        memset(ptr, 0, total);
        return ptr;
    }

    return al->calloc(al->ctx, num, size);
}

void *nad_realloc(nad_Al *al, void *ptr, size_t old_size, size_t new_size) {
    assert(al);
    assert(ptr || old_size == 0);

    if (new_size == 0) {
        nad_dealloc(al, ptr, old_size);
        return nullptr;
    }

    // fallback
    if (!al->realloc) {
        void *new_ptr = nad_alloc(al, new_size);
        if (!new_ptr) {
            return nullptr;
        }

        if (ptr) {
            const size_t copy_size = old_size < new_size ? old_size : new_size;
            memcpy(new_ptr, ptr, copy_size);
            nad_dealloc(al, ptr, old_size);
        }
        return new_ptr;
    }

    return al->realloc(al->ctx, ptr, old_size, new_size);
}

void nad_dealloc(nad_Al *al, void *ptr, size_t size) {
    assert(al);
    assert(al->dealloc);

    if (!ptr) {
        return;
    }

    al->dealloc(al->ctx, ptr, size);
}
