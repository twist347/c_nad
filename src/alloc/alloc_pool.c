#include "nad/alloc/alloc_pool.h"
#include "nad/core/util.h"
#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <string.h>

static constexpr size_t POOL_ALIGNMENT = alignof(max_align_t);

typedef struct PoolNode PoolNode;

struct PoolNode {
    PoolNode *next;
};

typedef struct {
    nad_Al *parent_al;
    char *data;
    PoolNode *free_head;
    size_t block_size;
    size_t block_count;
    size_t used;
} nad_AlPoolCtx;

static void *pool_alloc(void *ctx, size_t size);
static void *pool_calloc(void *ctx, size_t num, size_t size);
static void pool_dealloc(void *ctx, void *ptr, size_t size);

static void pool_build_free_list(nad_AlPoolCtx *ctx);
static bool pool_owns(const nad_AlPoolCtx *ctx, const void *ptr);

nad_Al *nad_al_pool_new(nad_Al *parent, size_t block_size, size_t block_count) {
    assert(parent);
    assert(block_size > 0);
    assert(block_count > 0);

    // each block must hold at least a free-list pointer
    if (block_size < sizeof(PoolNode)) {
        block_size = sizeof(PoolNode);
    }

    if (block_size > SIZE_MAX - (POOL_ALIGNMENT - 1)) {
        return nullptr;
    }

    // align up
    block_size = nad_align_up(block_size, POOL_ALIGNMENT);

    // allocate backing buffer
    size_t total_bytes;
    if (ckd_mul(&total_bytes, block_size, block_count)) {
        return nullptr;
    }

    // allocate context
    nad_AlPoolCtx *pool_ctx = nad_alloc(parent, sizeof(nad_AlPoolCtx));
    if (!pool_ctx) {
        return nullptr;
    }

    char *data = nad_alloc(parent, total_bytes);
    if (!data) {
        nad_dealloc(parent, pool_ctx, sizeof(nad_AlPoolCtx));
        return nullptr;
    }

    assert(nad_ptr_is_aligned(data, POOL_ALIGNMENT));

    pool_ctx->parent_al = parent;
    pool_ctx->data = data;
    pool_ctx->block_size = block_size;
    pool_ctx->block_count = block_count;
    pool_ctx->used = 0;
    pool_ctx->free_head = nullptr;

    pool_build_free_list(pool_ctx);

    // allocate the nad_Al itself
    nad_Al *al = nad_alloc(parent, sizeof(nad_Al));
    if (!al) {
        nad_dealloc(parent, data, total_bytes);
        nad_dealloc(parent, pool_ctx, sizeof(nad_AlPoolCtx));
        return nullptr;
    }

    al->ctx = pool_ctx;
    al->alloc = pool_alloc;
    al->calloc = pool_calloc;
    al->realloc = nullptr; // fallback in nad_al
    al->dealloc = pool_dealloc;

    return al;
}

void nad_al_pool_drop(nad_Al *al) {
    if (!al) {
        return;
    }

    assert(al->ctx);

    nad_AlPoolCtx *pool_ctx = al->ctx;
    nad_Al *parent_al = pool_ctx->parent_al;
    assert(parent_al);

    nad_dealloc(parent_al, pool_ctx->data, pool_ctx->block_size * pool_ctx->block_count);
    nad_dealloc(parent_al, pool_ctx, sizeof(nad_AlPoolCtx));
    nad_dealloc(parent_al, al, sizeof(nad_Al));
}

void nad_al_pool_reset(nad_Al *al) {
    assert(al);
    assert(al->ctx);

    nad_AlPoolCtx *pool_ctx = al->ctx;
    pool_ctx->used = 0;
    pool_build_free_list(pool_ctx);
}

nad_AlPoolStats nad_al_pool_stats(const nad_Al *al) {
    assert(al);
    assert(al->ctx);

    const nad_AlPoolCtx *pool_ctx = al->ctx;

    return (nad_AlPoolStats){
        .block_size = pool_ctx->block_size,
        .block_count = pool_ctx->block_count,
        .used = pool_ctx->used,
        .free = pool_ctx->block_count - pool_ctx->used,
    };
}

static void *pool_alloc(void *ctx, size_t size) {
    assert(ctx);

    if (size == 0) {
        return nullptr;
    }

    nad_AlPoolCtx *pool_ctx = ctx;
    if (size > pool_ctx->block_size) {
        return nullptr;
    }

    PoolNode *node = pool_ctx->free_head;
    if (!node) {
        return nullptr;
    }

    pool_ctx->free_head = node->next;
    ++pool_ctx->used;

    return node;
}

static void *pool_calloc(void *ctx, size_t num, size_t size) {
    assert(ctx);

    size_t total;
    if (ckd_mul(&total, num, size)) {
        return nullptr;
    }
    void *ptr = pool_alloc(ctx, total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

static void pool_dealloc(void *ctx, void *ptr, size_t size) {
    assert(ctx);
    NAD_UNUSED(size);

    if (!ptr) {
        return;
    }

    nad_AlPoolCtx *pool_ctx = ctx;

    assert(pool_owns(pool_ctx, ptr));
    assert(pool_ctx->used > 0);

    // push onto free list
    PoolNode *node = ptr;
    node->next = pool_ctx->free_head;
    pool_ctx->free_head = node;
    --pool_ctx->used;
}

static void pool_build_free_list(nad_AlPoolCtx *ctx) {
    assert(ctx);
    ctx->free_head = nullptr;

    // build list in reverse so that first alloc returns the first block
    for (size_t i = ctx->block_count; i > 0; --i) {
        PoolNode *node = (PoolNode *) (ctx->data + (i - 1) * ctx->block_size);
        node->next = ctx->free_head;
        ctx->free_head = node;
    }
}

static bool pool_owns(const nad_AlPoolCtx *ctx, const void *ptr) {
    const char *p = ptr;
    const char *begin = ctx->data;
    const char *end = ctx->data + ctx->block_size * ctx->block_count;

    if (p < begin || p >= end) {
        return false;
    }

    // must be aligned to block boundary
    const size_t offset = (size_t) (p - begin);
    return (offset % ctx->block_size) == 0;
}
