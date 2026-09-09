#pragma once

#include "terse/alloc/arena.h"

#include <unity.h>

#include <stddef.h>

/* ========== mods ========== */

/// burns the arena down to exactly 'want' free bytes, so the next
/// allocation over that size fails without a probe allocator
static inline void trs_test_arena_leave(trs_Al *arena, size_t want) {
    const size_t available = trs_al_arena_stats(arena).available;
    TEST_ASSERT_TRUE(available >= want);

    const size_t burn = available - want;
    if (burn > 0) {
        TEST_ASSERT_NOT_NULL(trs_alloc(arena, burn));
    }
    TEST_ASSERT_EQUAL_size_t(want, trs_al_arena_stats(arena).available);
}
