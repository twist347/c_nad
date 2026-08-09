#include "nad/mem/alloc_arena.h"
#include <assert.h>
#include <stddef.h>

constexpr size_t DEFAULT_ALIGNMENT = alignof(max_align_t);

static void *arena_alloc(void *ctx, size_t size);
static void *arena_calloc(void *ctx, size_t num, size_t size);
static void *arena_dealloc(void *ctx, void *ptr, size_t size);

/* ========== context ========== */

typedef struct {
    void *data;
    size_t cap;
    size_t offset;
} nad_AllocatorArenaCtx;

nad_Allocator *nad_allocator_arena_new(size_t cap) {

}

void nad_allocator_arena_drop(nad_Allocator *alloc) {

}

void nad_allocator_arena_reset(nad_Allocator *alloc) {

}

static void *arena_alloc(void *ctx, size_t size) {
    assert(ctx);
}

static void *arena_calloc(void *ctx, size_t num, size_t size) {

}

static void *arena_dealloc(void *ctx, void *ptr, size_t size) {

}
