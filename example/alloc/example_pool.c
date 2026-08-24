// for @snippet

#include "nad/alloc/default.h"
#include "nad/alloc/pool.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    nad_Al *pool = nad_al_pool_new(nad_al_default(), sizeof(int32_t), 3);
    if (!pool) {
        return 1;
    }

    // the block size asked for is a floor: it is rounded up to the alignment and to what
    // the free list needs, and the stats report what the blocks really are
    const nad_AlPoolStats st = nad_al_pool_stats(pool);
    printf("%zu blocks of %zu bytes, %zu free\n", st.block_count, st.block_size,
           st.free); // 3 blocks of 16 bytes, 3 free — asked for 4
    /// [build]

    /// [limits]
    // every block is the same size, so a request is served in a pointer move or not at all
    void *p = nullptr;
    for (size_t i = 0; i < st.block_count; ++i) {
        p = nad_alloc(pool, sizeof(int32_t));
    }
    printf("%zu free after taking them all\n", nad_al_pool_stats(pool).free); // 0

    // null now means the blocks ran out
    printf("%s\n", nad_alloc(pool, sizeof(int32_t)) ? "served" : "null: none left");

    // and null still means that after one comes back, if the request does not fit a block
    nad_dealloc(pool, p, sizeof(int32_t));
    printf("%s\n", nad_alloc(pool, st.block_size + 1) ? "served" : "null: over a block");
    /// [limits]

    nad_al_pool_drop(pool);

    return 0;
}
