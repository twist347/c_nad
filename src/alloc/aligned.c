#include "terse/alloc/aligned.h"

#include "internal/ptr.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* ========== internals ========== */

[[nodiscard]]
static void *aligned_alloc_block(void *ctx, size_t size);

static void aligned_dealloc_block(void *ctx, void *ptr, size_t size);

[[nodiscard]]
static size_t parent_size(size_t size, size_t alignment);

typedef struct {
    trs_Al *parent_al;
    size_t alignment;
} AlignedCtx;

#define ASSERT_ALIGNED(al)                       \
    (assert(al),                                 \
     assert((al)->alloc == aligned_alloc_block), \
     assert((al)->ctx))

/* ========== lifetime ========== */

trs_Al *trs_al_aligned_new(trs_Al *parent, size_t alignment) {
    assert(parent);
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0);
    assert(alignment >= TRS_DEFAULT_ALIGNMENT);

    AlignedCtx *aligned_ctx = trs_alloc(parent, sizeof(AlignedCtx));
    if (!aligned_ctx) {
        return nullptr;
    }

    trs_Al *obj = trs_alloc(parent, sizeof(trs_Al));
    if (!obj) {
        trs_dealloc(parent, aligned_ctx, sizeof(AlignedCtx));
        return nullptr;
    }

    aligned_ctx->parent_al = parent;
    aligned_ctx->alignment = alignment;

    obj->ctx = aligned_ctx;
    obj->alloc = aligned_alloc_block;
    obj->calloc = nullptr;
    obj->realloc = nullptr;
    obj->dealloc = aligned_dealloc_block;

    return obj;
}

void trs_al_aligned_drop(trs_Al *self) {
    if (!self) {
        return;
    }

    ASSERT_ALIGNED(self);

    AlignedCtx *aligned_ctx = self->ctx;
    trs_Al *parent_al = aligned_ctx->parent_al;
    assert(parent_al);

    trs_dealloc(parent_al, aligned_ctx, sizeof(AlignedCtx));
    trs_dealloc(parent_al, self, sizeof(trs_Al));
}

/* ========== internals ========== */

static void *aligned_alloc_block(void *ctx, size_t size) {
    assert(ctx);

    AlignedCtx *aligned_ctx = ctx;
    const size_t alignment = aligned_ctx->alignment;

    if (size == 0) {
        return nullptr;
    }

    if (size > SIZE_MAX - (alignment - 1 + sizeof(void *))) {
        return nullptr;
    }

    unsigned char *base = trs_alloc(aligned_ctx->parent_al, parent_size(size, alignment));
    if (!base) {
        return nullptr;
    }

    unsigned char *ptr = trs_ptr_align_up(base + sizeof(void *), alignment);

    ((void **) ptr)[-1] = base;

    assert(trs_ptr_is_aligned(ptr, alignment));

    return ptr;
}

static void aligned_dealloc_block(void *ctx, void *ptr, size_t size) {
    assert(ctx);
    assert(ptr);

    AlignedCtx *aligned_ctx = ctx;

    void *base = ((void **) ptr)[-1];
    assert((unsigned char *) base <= (unsigned char *) ptr);

    trs_dealloc(aligned_ctx->parent_al, base, parent_size(size, aligned_ctx->alignment));
}

static size_t parent_size(size_t size, size_t alignment) {
    return size + alignment - 1 + sizeof(void *);
}
