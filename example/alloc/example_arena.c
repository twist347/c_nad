// for @snippet

#include "terse/alloc/arena.h"
#include "terse/alloc/default.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the block comes from the parent once, and is handed out in pieces from there on
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    if (!arena) {
        return 1;
    }

    int rc = 1;
    int32_t *a = TRS_ALLOC(int32_t, arena, 4);
    int32_t *b = TRS_ALLOC(int32_t, arena, 4);
    if (!a || !b) {
        goto drop;
    }

    const trs_AlArenaStats st = trs_al_arena_stats(arena);
    printf("%zu of %zu used, %zu left\n", st.used, st.cap,
           st.available); // 32 of 1024 used, 992 left — each piece is padded to the alignment
    /// [build]

    /// [reset]
    // giving one piece back does nothing at all: an arena has no per-block free
    TRS_DEALLOC(int32_t, arena, a, 4);
    printf("%zu used after a dealloc\n", trs_al_arena_stats(arena).used); // 32

    // this is how the memory comes back — all of it at once, and 'a' and 'b' are dead
    trs_al_arena_reset(arena);
    printf("%zu used after the reset\n", trs_al_arena_stats(arena).used); // 0
    /// [reset]

    rc = 0;
drop:
    trs_al_arena_drop(arena);

    return rc;
}
