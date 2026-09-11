#include "tda/alloc/arena.h"

#include "tda/core/util.h"

#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <stddef.h>
#include <string.h>

/* ========== internals ========== */

[[nodiscard]]
static void *arena_alloc(void *ctx, size_t size);

/// grows or shrinks the last block where it stands, since nothing lies past it; any
/// other block moves, and the old one stays behind like every block the arena has lent
[[nodiscard]]
static void *arena_realloc(void *ctx, void *ptr, size_t old_size, size_t new_size);

static void arena_dealloc(void *ctx, void *ptr, size_t size);

typedef struct {
    tda_Al *parent_al;
    void *data;
    size_t cap;
    size_t offset;
} ArenaCtx;

/// whether the block at 'ptr', 'size' bytes when it was handed out, is the last one: the
/// only block with nothing but the free tail after it
[[nodiscard]]
static bool is_last_block(const ArenaCtx *arena_ctx, const void *ptr, size_t size);

#define ASSERT_ARENA(al)                 \
    (assert(al),                         \
     assert((al)->alloc == arena_alloc), \
     assert((al)->ctx))

/* ========== lifetime ========== */

tda_Al *tda_al_arena_new(tda_Al *parent, size_t cap) {
    assert(parent);
    assert(cap > 0);

    void *data = tda_alloc(parent, cap);
    if (!data) {
        return nullptr;
    }

    ArenaCtx *arena_ctx = tda_alloc(parent, sizeof(ArenaCtx));
    if (!arena_ctx) {
        tda_dealloc(parent, data, cap);
        return nullptr;
    }

    tda_Al *obj = tda_alloc(parent, sizeof(tda_Al));
    if (!obj) {
        tda_dealloc(parent, arena_ctx, sizeof(ArenaCtx));
        tda_dealloc(parent, data, cap);
        return nullptr;
    }

    assert(tda_ptr_is_aligned(data, TDA_DEFAULT_ALIGNMENT));

    arena_ctx->parent_al = parent;
    arena_ctx->data = data;
    arena_ctx->cap = cap;
    arena_ctx->offset = 0;

    obj->ctx = arena_ctx;
    obj->alloc = arena_alloc;
    obj->calloc = nullptr;
    obj->realloc = arena_realloc;
    obj->dealloc = arena_dealloc;

    return obj;
}

void tda_al_arena_drop(tda_Al *self) {
    if (!self) {
        return;
    }

    ASSERT_ARENA(self);

    ArenaCtx *arena_ctx = self->ctx;
    tda_Al *parent_al = arena_ctx->parent_al;
    assert(parent_al);

    tda_dealloc(parent_al, arena_ctx->data, arena_ctx->cap);
    tda_dealloc(parent_al, arena_ctx, sizeof(ArenaCtx));
    tda_dealloc(parent_al, self, sizeof(tda_Al));
}

/* ========== mods ========== */

void tda_al_arena_reset(tda_Al *self) {
    ASSERT_ARENA(self);

    ArenaCtx *arena_ctx = self->ctx;
    arena_ctx->offset = 0;
}

/* ========== stats ========== */

tda_AlArenaStats tda_al_arena_stats(const tda_Al *self) {
    ASSERT_ARENA(self);

    const ArenaCtx *arena_ctx = self->ctx;

    return (tda_AlArenaStats){
        .cap = arena_ctx->cap,
        .used = arena_ctx->offset,
        .available = arena_ctx->cap - arena_ctx->offset,
    };
}

/* ========== internals ========== */

static void *arena_alloc(void *ctx, size_t size) {
    assert(ctx);

    ArenaCtx *arena_ctx = ctx;

    if (size == 0) {
        return nullptr;
    }

    size_t aligned_size;
    size_t end;
    if (tda_ckd_align_up(&aligned_size, size, TDA_DEFAULT_ALIGNMENT)
        || ckd_add(&end, arena_ctx->offset, aligned_size)
        || end > arena_ctx->cap) {
        return nullptr;
    }

    void *ptr = tda_byte_offset_mut(arena_ctx->data, 1, arena_ctx->offset);
    arena_ctx->offset += aligned_size;

    return ptr;
}

static void *arena_realloc(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    assert(ctx);
    assert(new_size > 0); // tda_realloc answers a request for nothing itself

    ArenaCtx *arena_ctx = ctx;

    if (!ptr) {
        return arena_alloc(ctx, new_size);
    }

    if (is_last_block(arena_ctx, ptr, old_size)) {
        const size_t start = arena_ctx->offset - tda_align_up(old_size, TDA_DEFAULT_ALIGNMENT);
        size_t aligned_size;
        size_t end;
        if (tda_ckd_align_up(&aligned_size, new_size, TDA_DEFAULT_ALIGNMENT) ||
            ckd_add(&end, start, aligned_size) ||
            end > arena_ctx->cap
        ) {
            return nullptr;
        }
        arena_ctx->offset = end;
        return ptr;
    }

    void *new_ptr = arena_alloc(ctx, new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
    }
    return new_ptr;
}

static bool is_last_block(const ArenaCtx *arena_ctx, const void *ptr, size_t size) {
    const size_t start = (size_t) tda_byte_diff(ptr, arena_ctx->data);
    return start + tda_align_up(size, TDA_DEFAULT_ALIGNMENT) == arena_ctx->offset;
}

static void arena_dealloc(void *ctx, void *ptr, size_t size) {
    // arena doesn't free individual allocations

    TDA_UNUSED(ctx);
    TDA_UNUSED(ptr);
    TDA_UNUSED(size);
}
