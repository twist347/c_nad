#include "tda/alloc/pool.h"

#include "tda/core/util.h"

#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <stdint.h>

/* ========== internals ========== */

typedef struct PoolNode PoolNode;

struct PoolNode {
    PoolNode *next;
};

typedef struct {
    tda_Al *parent_al;
    unsigned char *data;
    PoolNode *free_head;
    size_t block_size;
    size_t block_count;
    size_t used;
} PoolCtx;

[[nodiscard]]
static void *pool_alloc(void *ctx, size_t size);

static void pool_dealloc(void *ctx, void *ptr, size_t size);

static void pool_build_free_list(PoolCtx *ctx);

[[nodiscard]] [[maybe_unused]]
static bool pool_owns(const PoolCtx *ctx, const void *ptr);

#define ASSERT_POOL(al)                 \
    (assert(al),                        \
     assert((al)->alloc == pool_alloc), \
     assert((al)->ctx))

/* ========== lifetime ========== */

tda_Al *tda_al_pool_new(tda_Al *parent, size_t block_size, size_t block_count) {
    assert(parent);
    assert(block_size > 0);
    assert(block_count > 0);

    // each block must hold at least a free-list pointer
    if (block_size < sizeof(PoolNode)) {
        block_size = sizeof(PoolNode);
    }

    if (tda_ckd_align_up(&block_size, block_size, TDA_DEFAULT_ALIGNMENT)) {
        return nullptr;
    }

    // allocate backing buffer
    size_t total_bytes;
    if (ckd_mul(&total_bytes, block_size, block_count)) {
        return nullptr;
    }

    // allocate context
    PoolCtx *pool_ctx = tda_alloc(parent, sizeof(PoolCtx));
    if (!pool_ctx) {
        return nullptr;
    }

    unsigned char *data = tda_alloc(parent, total_bytes);
    if (!data) {
        tda_dealloc(parent, pool_ctx, sizeof(PoolCtx));
        return nullptr;
    }

    assert(tda_ptr_is_aligned(data, TDA_DEFAULT_ALIGNMENT));

    pool_ctx->parent_al = parent;
    pool_ctx->data = data;
    pool_ctx->block_size = block_size;
    pool_ctx->block_count = block_count;
    pool_ctx->used = 0;
    pool_ctx->free_head = nullptr;

    pool_build_free_list(pool_ctx);

    // allocate the tda_Al itself
    tda_Al *obj = tda_alloc(parent, sizeof(tda_Al));
    if (!obj) {
        tda_dealloc(parent, data, total_bytes);
        tda_dealloc(parent, pool_ctx, sizeof(PoolCtx));
        return nullptr;
    }

    obj->ctx = pool_ctx;
    obj->alloc = pool_alloc;
    obj->calloc = nullptr;
    obj->realloc = nullptr;
    obj->dealloc = pool_dealloc;

    return obj;
}

void tda_al_pool_drop(tda_Al *self) {
    if (!self) {
        return;
    }

    ASSERT_POOL(self);

    PoolCtx *pool_ctx = self->ctx;
    tda_Al *parent_al = pool_ctx->parent_al;
    assert(parent_al);

    tda_dealloc(parent_al, pool_ctx->data, pool_ctx->block_size * pool_ctx->block_count);
    tda_dealloc(parent_al, pool_ctx, sizeof(PoolCtx));
    tda_dealloc(parent_al, self, sizeof(tda_Al));
}

void tda_al_pool_reset(tda_Al *self) {
    ASSERT_POOL(self);

    PoolCtx *pool_ctx = self->ctx;
    pool_ctx->used = 0;
    pool_build_free_list(pool_ctx);
}

tda_AlPoolStats tda_al_pool_stats(const tda_Al *self) {
    ASSERT_POOL(self);

    const PoolCtx *pool_ctx = self->ctx;

    return (tda_AlPoolStats){
        .block_size = pool_ctx->block_size,
        .block_count = pool_ctx->block_count,
        .used = pool_ctx->used,
        .free = pool_ctx->block_count - pool_ctx->used,
    };
}

/* ========== internals ========== */

static void *pool_alloc(void *ctx, size_t size) {
    assert(ctx);

    if (size == 0) {
        return nullptr;
    }

    PoolCtx *pool_ctx = ctx;
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

static void pool_dealloc(void *ctx, void *ptr, size_t size) {
    assert(ctx);
    TDA_UNUSED(size);

    if (!ptr) {
        return;
    }

    PoolCtx *pool_ctx = ctx;

    assert(pool_owns(pool_ctx, ptr));
    assert(pool_ctx->used > 0);

    // push onto free list
    PoolNode *node = ptr;
    node->next = pool_ctx->free_head;
    pool_ctx->free_head = node;
    --pool_ctx->used;
}

static void pool_build_free_list(PoolCtx *ctx) {
    assert(ctx);
    ctx->free_head = nullptr;

    // build list in reverse so that first alloc returns the first block
    for (size_t i = ctx->block_count; i > 0; --i) {
        PoolNode *node = (PoolNode *) tda_byte_offset_mut(ctx->data, ctx->block_size, i - 1);
        node->next = ctx->free_head;
        ctx->free_head = node;
    }
}

static bool pool_owns(const PoolCtx *ctx, const void *ptr) {
    // addresses, not pointers: ordering a pointer from outside the pool against the pool's
    // own is undefined, and one from outside is exactly what this is here to catch
    const uintptr_t addr = (uintptr_t) ptr;
    const uintptr_t begin = (uintptr_t) ctx->data;
    const uintptr_t end = begin + ctx->block_size * ctx->block_count;

    return addr >= begin && addr < end && (addr - begin) % ctx->block_size == 0;
}
