// for @snippet

#include "nad/alloc/alloc.h"
#include "nad/alloc/default.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/// [custom]
// a table must fill in alloc and dealloc, and may leave calloc and realloc null — the
// wrappers build those two out of the other two. ctx is whatever the implementation
// keeps, and it comes back to every callback
typedef struct {
    nad_Al *parent;
    size_t live;
} CountingCtx;

static void *counting_alloc(void *ctx, size_t size) {
    CountingCtx *c = ctx;

    void *ptr = nad_alloc(c->parent, size);
    if (ptr) {
        c->live += size;
    }

    return ptr;
}

static void counting_dealloc(void *ctx, void *ptr, size_t size) {
    CountingCtx *c = ctx;

    nad_dealloc(c->parent, ptr, size);
    c->live -= size;
}

/// [custom]

int main() {
    /// [use]
    nad_Al *al = nad_al_default();

    // the macros carry sizeof(T) and check the multiplication, so a count that would
    // overflow gives null rather than a wrapped-around request
    int32_t *xs = NAD_ALLOC(int32_t, al, 4);
    if (!xs) {
        return 1;
    }

    xs[0] = 7;
    printf("%" PRId32 "\n", xs[0]); // 7

    NAD_DEALLOC(int32_t, al, xs, 4);
    /// [use]

    /// [wrap]
    // an allocator built over another borrows it: the default one outlives this, and
    // nothing here owns it
    CountingCtx ctx = {.parent = nad_al_default(), .live = 0};
    nad_Al counting = {
        .ctx = &ctx,
        .alloc = counting_alloc,
        .dealloc = counting_dealloc,
    };

    // calloc is null in that table, so this is alloc and memset done by the wrapper
    int32_t *zeros = NAD_CALLOC(int32_t, &counting, 4);
    if (!zeros) {
        return 1;
    }
    printf("%" PRId32 ", %zu bytes live\n", zeros[0], ctx.live); // 0, 16 bytes live

    NAD_DEALLOC(int32_t, &counting, zeros, 4);
    printf("%zu bytes live\n", ctx.live); // 0 bytes live
    /// [wrap]

    return 0;
}
