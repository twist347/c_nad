#include "terse/alloc/alloc.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

/* ========== wrappers ========== */

void *trs_alloc(trs_Al *al, size_t size) {
    assert(al);
    assert(al->alloc);

    if (size == 0) {
        return nullptr;
    }

    return al->alloc(al->ctx, size);
}

void *trs_calloc(trs_Al *al, size_t num, size_t size) {
    assert(al);

    if (num == 0 || size == 0) {
        return nullptr;
    }

    size_t total;
    if (ckd_mul(&total, num, size)) {
        return nullptr;
    }

    // fallback
    if (!al->calloc) {
        void *ptr = trs_alloc(al, total);
        if (!ptr) {
            return nullptr;
        }
        memset(ptr, 0, total);
        return ptr;
    }

    return al->calloc(al->ctx, num, size);
}

void *trs_realloc(trs_Al *al, void *ptr, size_t old_size, size_t new_size) {
    assert(al);
    assert(ptr || old_size == 0);

    if (new_size == 0) {
        trs_dealloc(al, ptr, old_size);
        return nullptr;
    }

    // fallback
    if (!al->realloc) {
        void *new_ptr = trs_alloc(al, new_size);
        if (!new_ptr) {
            return nullptr;
        }

        if (ptr) {
            const size_t copy_size = old_size < new_size ? old_size : new_size;
            memcpy(new_ptr, ptr, copy_size);
            trs_dealloc(al, ptr, old_size);
        }
        return new_ptr;
    }

    return al->realloc(al->ctx, ptr, old_size, new_size);
}

void trs_dealloc(trs_Al *al, void *ptr, size_t size) {
    assert(al);
    assert(al->dealloc);

    if (!ptr) {
        return;
    }

    al->dealloc(al->ctx, ptr, size);
}
