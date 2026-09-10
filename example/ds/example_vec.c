// for @snippet

#include "trs/algo/modify.h"
#include "trs/algo/sort.h"
#include "trs/alloc/arena.h"
#include "trs/alloc/default.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"
#include "trs/ds/vec.h"

#include <inttypes.h>
#include <stdio.h>

// the recipe trs_vec_swap points at. Two vecs on different allocators cannot be exchanged
// in place — a block belongs to the allocator that made it — so each side is copied onto
// the other's allocator and the copies are handed over
/// [allocators]
[[nodiscard]]
static trs_Status exchange(trs_Vec *a, trs_Vec *b) {
    trs_Vec *for_a = nullptr;
    trs_Vec *for_b = nullptr;

    // both copies are made before either vec is touched, so a refusal here leaves the
    // pair exactly as it was
    trs_Status st = trs_vec_copy_with(b, trs_vec_al(a), &for_a);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    st = trs_vec_copy_with(a, trs_vec_al(b), &for_b);
    if (TRS_STATUS_IS_ERR(st)) {
        goto out;
    }

    // from here every move stays inside one allocator: the block changes hands, and the
    // status is still checked because the operation as such can fail, not this call
    st = trs_vec_move_assign(for_a, a);
    if (TRS_STATUS_IS_ERR(st)) {
        goto out;
    }
    st = trs_vec_move_assign(for_b, b);

out:
    // each copy is left empty by its move and is dropped either way
    trs_vec_drop(for_a);
    trs_vec_drop(for_b);

    return st;
}
/// [allocators]

// the pair the recipe is shown on: one vec on the default allocator, one in an arena
static int show_exchange() {
    trs_Al *al = trs_al_default();

    trs_Al *arena = trs_al_arena_new(al, 1024);
    if (!arena) {
        return 1;
    }

    trs_Vec *a = nullptr;
    trs_Vec *b = nullptr;
    int rc = 1;

    if (TRS_STATUS_IS_ERR(TRS_VEC_OF(int32_t, al, &a, 1, 2, 3))) {
        goto out;
    }
    if (TRS_STATUS_IS_ERR(TRS_VEC_OF(int32_t, arena, &b, 7, 8))) {
        goto out;
    }

    if (TRS_STATUS_IS_ERR(exchange(a, b))) {
        goto out;
    }
    trs_vec_print(a, trs_fprint_i32); // [7, 8]
    trs_vec_print(b, trs_fprint_i32); // [1, 2, 3]

    rc = 0;
out:
    trs_vec_drop(a);
    trs_vec_drop(b);
    trs_al_arena_drop(arena);

    return rc;
}

int main() {
    if (show_exchange() != 0) {
        return 1;
    }

    /// [build]
    // the handle comes back through 'out', and the status cannot be ignored
    trs_Al *al = trs_al_default();

    trs_Vec *v = nullptr;
    if (TRS_STATUS_IS_ERR(TRS_VEC_OF(int32_t, al, &v, 5, 3, 1))) {
        return 1;
    }

    printf("%zu of %zu\n", trs_vec_len(v), trs_vec_cap(v)); // 3 of 3
    /// [build]

    /// [grow]
    // the block doubles when it fills, so a run of pushes costs O(1) amortized per elem.
    // Every op that may reallocate returns a status, and C has no defer: once the vec is
    // held, the failure path jumps to a common exit instead of returning early
    int rc = 1;

    if (TRS_STATUS_IS_ERR(TRS_VEC_PUSH(int32_t, v, 4))) {
        goto out;
    }
    printf("%zu of %zu\n", trs_vec_len(v), trs_vec_cap(v)); // 4 of 6

    // a pop moves the length and leaves the room alone
    trs_vec_pop(v);
    printf("%zu of %zu\n", trs_vec_len(v), trs_vec_cap(v)); // 3 of 6

    // shrink_to_fit is how the room goes back, and it can fail like any other resize
    if (TRS_STATUS_IS_ERR(trs_vec_shrink_to_fit(v))) {
        goto out;
    }
    printf("%zu of %zu\n", trs_vec_len(v), trs_vec_cap(v)); // 3 of 3
    /// [grow]

    /// [bulk]
    // extend takes the room once for the whole run rather than once per elem, and
    // insert_span moves the tail once where a loop of insert would move it per elem
    if (TRS_STATUS_IS_ERR(TRS_VEC_EXTEND(int32_t, v, 4, 2))) {
        goto out;
    }
    trs_vec_print(v, trs_fprint_i32); // [5, 3, 1, 4, 2]

    if (TRS_STATUS_IS_ERR(TRS_VEC_INSERT_SPAN(int32_t, v, 1, 9, 9))) {
        goto out;
    }
    trs_vec_print(v, trs_fprint_i32); // [5, 9, 9, 3, 1, 4, 2]

    // and the way back out, which allocates nothing and so cannot fail
    trs_vec_remove_range(v, 1, 2);
    trs_vec_print(v, trs_fprint_i32); // [5, 3, 1, 4, 2]
    /// [bulk]

    /// [algo]
    // the bridge to algo runs both ways. Out: a writable view, and algo rearranges the
    // elems in place — a vec keeps no order of its own to protect
    trs_span_sort(trs_vec_to_span_mut(v), trs_cmp_i32);
    trs_vec_print(v, trs_fprint_i32); // [1, 2, 3, 4, 5]

    // back: an algorithm that drops elems packs the kept ones to the front and returns
    // how many there are, and the vec adopts that length
    const size_t kept = trs_span_remove(trs_vec_to_span_mut(v), &(int32_t){3}, trs_eq_i32);
    if (TRS_STATUS_IS_ERR(trs_vec_resize(v, kept))) {
        goto out;
    }
    trs_vec_print(v, trs_fprint_i32); // [1, 2, 4, 5]

    printf(
        "%" PRId32 " .. %" PRId32 "\n", *TRS_VEC_FRONT_AS(int32_t, v),
        *TRS_VEC_BACK_AS(int32_t, v)
    ); // 1 .. 5

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    trs_vec_drop(v);
    return rc;
    /// [algo]
}
