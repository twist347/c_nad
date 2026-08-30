#include "nad/alloc/aligned.h"

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
    nad_Al *parent_al;
    size_t alignment;
} AlignedCtx;

/* ========== lifetime ========== */

nad_Al *nad_al_aligned_new(nad_Al *parent, size_t alignment) {
    assert(parent);
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0);
    assert(alignment >= alignof(max_align_t));

    AlignedCtx *aligned_ctx = nad_alloc(parent, sizeof(AlignedCtx));
    if (!aligned_ctx) {
        return nullptr;
    }

    nad_Al *al = nad_alloc(parent, sizeof(nad_Al));
    if (!al) {
        nad_dealloc(parent, aligned_ctx, sizeof(AlignedCtx));
        return nullptr;
    }

    aligned_ctx->parent_al = parent;
    aligned_ctx->alignment = alignment;

    al->ctx = aligned_ctx;
    al->alloc = aligned_alloc_block;
    al->calloc = nullptr;
    al->realloc = nullptr;
    al->dealloc = aligned_dealloc_block;

    return al;
}

void nad_al_aligned_drop(nad_Al *al) {
    if (!al) {
        return;
    }

    assert(al->ctx);

    AlignedCtx *aligned_ctx = al->ctx;
    nad_Al *parent_al = aligned_ctx->parent_al;
    assert(parent_al);

    nad_dealloc(parent_al, aligned_ctx, sizeof(AlignedCtx));
    nad_dealloc(parent_al, al, sizeof(nad_Al));
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

    unsigned char *base = nad_alloc(aligned_ctx->parent_al, parent_size(size, alignment));
    if (!base) {
        return nullptr;
    }

    const uintptr_t addr = (uintptr_t) (base + sizeof(void *));
    unsigned char *ptr = (unsigned char *) ((addr + (alignment - 1)) & ~(alignment - 1));

    ((void **) ptr)[-1] = base;

    assert(nad_ptr_is_aligned(ptr, alignment));

    return ptr;
}

static void aligned_dealloc_block(void *ctx, void *ptr, size_t size) {
    assert(ctx);
    assert(ptr);

    AlignedCtx *aligned_ctx = ctx;

    void *base = ((void **) ptr)[-1];
    assert((unsigned char *) base <= (unsigned char *) ptr);

    nad_dealloc(aligned_ctx->parent_al, base, parent_size(size, aligned_ctx->alignment));
}

static size_t parent_size(size_t size, size_t alignment) {
    return size + alignment - 1 + sizeof(void *);
}
