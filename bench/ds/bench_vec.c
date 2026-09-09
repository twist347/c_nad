// push is where the growth policy shows: every benchmark here pushes the same elems, and
// the pair "grows" / "reserved" says what the growing itself cost on that allocator

#include "terse/alloc/arena.h"
#include "terse/alloc/default.h"
#include "terse/ds/vec.h"

#include <ubench.h>

#include <stdint.h>

static constexpr size_t PUSHES = 100'000;

// the arena has to hold every block the growth walks through, not just the last one:
// its dealloc is a no-op, so nothing is ever handed back
static constexpr size_t ARENA_CAP = 4 * 1024 * 1024;

/// pushes PUSHES elems into a fresh vec on 'al'. A non-zero 'cap' asks for the room up
/// front, so the difference between the two is the cost of growing
static void push_all(trs_Al *al, size_t cap) {
    trs_Vec *v = nullptr;
    if (TRS_STATUS_IS_ERR(TRS_VEC_NEW(int32_t, al, &v))) {
        return;
    }

    if (cap > 0 && TRS_STATUS_IS_ERR(trs_vec_reserve(v, cap))) {
        trs_vec_drop(v);
        return;
    }

    for (size_t i = 0; i < PUSHES; ++i) {
        if (TRS_STATUS_IS_ERR(TRS_VEC_PUSH(int32_t, v, (int32_t) i))) {
            break;
        }
    }

    UBENCH_DO_NOTHING(trs_vec_data_mut(v));
    trs_vec_drop(v);
}

UBENCH(push, default_grows) {
    push_all(trs_al_default(), 0);
}

UBENCH(push, default_reserved) {
    push_all(trs_al_default(), PUSHES);
}

// the arena has no realloc of its own, so every growth is take-copy-drop and the dropped
// block stays dead until the arena is reset — the case the growth factor matters most in
UBENCH(push, arena_grows) {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), ARENA_CAP);
    if (!arena) {
        return;
    }

    push_all(arena, 0);
    trs_al_arena_drop(arena);
}

UBENCH(push, arena_reserved) {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), ARENA_CAP);
    if (!arena) {
        return;
    }

    push_all(arena, PUSHES);
    trs_al_arena_drop(arena);
}

UBENCH_MAIN()
