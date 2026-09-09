// for @snippet

#include "terse/algo/search.h"
#include "terse/algo/sort.h"
#include "terse/alloc/arena.h"
#include "terse/alloc/default.h"
#include "terse/core/cmp.h"
#include "terse/core/print.h"
#include "terse/ds/arr.h"

#include <inttypes.h>
#include <stdio.h>

// an equality that sees less than the bytes do: two elems equal under it can still
// differ byte for byte
static bool eq_abs_i32(const void *lhs, const void *rhs) {
    const int32_t a = *(const int32_t *) lhs;
    const int32_t b = *(const int32_t *) rhs;

    return (a < 0 ? -a : a) == (b < 0 ? -b : b);
}

int main() {
    /// [build]
    // the handle comes back through 'out', and the status cannot be ignored
    trs_Al *al = trs_al_default();

    trs_Arr *a = nullptr;
    if (TRS_STATUS_IS_ERR(TRS_ARR_OF(int32_t, al, &a, 5, 3, 1, 4, 2))) {
        return 1;
    }
    /// [build]

    /// [compare]
    // two arrs are equal when they hold the same elems: the same length, the same bytes
    trs_Arr *twin = nullptr;
    if (TRS_STATUS_IS_ERR(TRS_ARR_OF(int32_t, al, &twin, 5, 3, 1, 4, 2))) {
        trs_arr_drop(a);
        return 1;
    }
    printf("%d\n", trs_arr_eq(a, twin)); // 1

    // an elem whose equality is not its bytes needs the other form, which asks a trs_Eq
    TRS_ARR_SET(int32_t, twin, 0, -5);
    printf("%d %d\n", trs_arr_eq(a, twin), trs_arr_eq_by(a, twin, eq_abs_i32)); // 0 1

    trs_arr_drop(twin);
    /// [compare]

    /// [algo]
    // an arr has no order of its own to protect, so algo rearranges the elems in place
    trs_span_sort(trs_arr_to_span_mut(a), trs_cmp_i32);

    size_t idx;
    if (trs_span_binary_search(trs_arr_to_span(a), &(int32_t){4}, trs_cmp_i32, &idx)) {
        printf("4 is at %zu\n", idx); // 4 is at 3
    }
    /// [algo]

    /// [access]
    TRS_ARR_SET(int32_t, a, 0, 0);
    printf("%" PRId32 " .. %" PRId32 " over %zu elems\n", *TRS_ARR_FRONT_AS(int32_t, a),
           *TRS_ARR_BACK_AS(int32_t, a), trs_arr_len(a)); // 0 .. 5 over 5 elems
    /// [access]

    /// [copy]
    // every op that allocates can fail, and C has no defer: once a resource is held, the
    // failure path jumps to a common exit instead of returning early
    trs_Arr *copy = nullptr;
    trs_Arr *shorter = nullptr;
    trs_Arr *in_arena = nullptr;
    trs_Al *arena = nullptr;
    int rc = 1;

    // one way to copy: a fresh arr with the same elems, on the same allocator as 'a'
    if (TRS_STATUS_IS_ERR(trs_arr_copy(a, &copy))) {
        goto out;
    }
    trs_arr_print(copy, trs_fprint_i32); // [0, 2, 3, 4, 5]

    // the other: an arr that already exists is overwritten, and its block is resized to
    // whatever the source needs — here from two elems to five
    if (TRS_STATUS_IS_ERR(TRS_ARR_NEW_LEN(int32_t, 2, al, &shorter))) {
        goto out;
    }
    trs_arr_print(shorter, trs_fprint_i32); // [0, 0] — new_len zeroes the block

    if (TRS_STATUS_IS_ERR(trs_arr_copy_assign(a, shorter))) {
        goto out;
    }
    trs_arr_print(shorter, trs_fprint_i32); // [0, 2, 3, 4, 5]

    // a copy is born where its source lives; copy_with names another allocator instead.
    // The arena bumps a pointer and gives everything back at once, so what it holds must
    // not outlive it
    arena = trs_al_arena_new(al, 1024);
    if (!arena) {
        goto out;
    }

    if (TRS_STATUS_IS_ERR(trs_arr_copy_with(a, arena, &in_arena))) {
        goto out;
    }
    trs_arr_print(in_arena, trs_fprint_i32); // [0, 2, 3, 4, 5]

    // a move hands the elems over and leaves the source empty. These two sit on different
    // allocators, so it costs n and may refuse; on one it would be a handover that cannot
    if (TRS_STATUS_IS_ERR(trs_arr_move_assign(in_arena, shorter))) {
        goto out;
    }
    printf("%zu <- %zu\n", trs_arr_len(shorter), trs_arr_len(in_arena)); // 5 <- 0

    // and the one operation that moves a block: the two are exchanged whole, lengths and
    // all, so a pointer into either of them now points into the other. It wants both on
    // one allocator — 'a' and 'shorter' are on 'al' — since it exchanges the blocks where
    // they lie instead of copying anything, and so has nothing to report
    trs_arr_swap(a, shorter);

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above. What the arena
    // gave out goes back before the arena itself
    trs_arr_drop(in_arena);
    trs_al_arena_drop(arena);

    trs_arr_drop(shorter);
    trs_arr_drop(copy);
    trs_arr_drop(a);
    return rc;
    /// [copy]
}
