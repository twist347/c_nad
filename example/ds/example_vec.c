// for @snippet

#include "nad/algo/modify.h"
#include "nad/algo/sort.h"
#include "nad/alloc/arena.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/ds/vec.h"

#include <inttypes.h>
#include <stdio.h>

// the recipe nad_vec_swap points at. Two vecs on different allocators cannot be exchanged
// in place — a block belongs to the allocator that made it — so each side is copied onto
// the other's allocator and the copies are handed over
/// [allocators]
[[nodiscard]]
static nad_Status exchange(nad_Vec *a, nad_Vec *b) {
    nad_Vec *for_a = nullptr;
    nad_Vec *for_b = nullptr;

    // both copies are made before either vec is touched, so a refusal here leaves the
    // pair exactly as it was
    nad_Status st = nad_vec_copy_with(b, nad_vec_al(a), &for_a);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    st = nad_vec_copy_with(a, nad_vec_al(b), &for_b);
    if (NAD_STATUS_IS_ERR(st)) {
        goto out;
    }

    // from here every move stays inside one allocator: the block changes hands, and the
    // status is still checked because the operation as such can fail, not this call
    st = nad_vec_move_assign(for_a, a);
    if (NAD_STATUS_IS_ERR(st)) {
        goto out;
    }
    st = nad_vec_move_assign(for_b, b);

out:
    // each copy is left empty by its move and is dropped either way
    nad_vec_drop(for_a);
    nad_vec_drop(for_b);

    return st;
}
/// [allocators]

// the pair the recipe is shown on: one vec on the default allocator, one in an arena
static int show_exchange() {
    nad_Al *al = nad_al_default();

    nad_Al *arena = nad_al_arena_new(al, 1024);
    if (!arena) {
        return 1;
    }

    nad_Vec *a = nullptr;
    nad_Vec *b = nullptr;
    int rc = 1;

    if (NAD_STATUS_IS_ERR(NAD_VEC_OF(int32_t, al, &a, 1, 2, 3))) {
        goto out;
    }
    if (NAD_STATUS_IS_ERR(NAD_VEC_OF(int32_t, arena, &b, 7, 8))) {
        goto out;
    }

    if (NAD_STATUS_IS_ERR(exchange(a, b))) {
        goto out;
    }
    nad_vec_print(a, nad_fprint_i32); // [7, 8]
    nad_vec_print(b, nad_fprint_i32); // [1, 2, 3]

    rc = 0;
out:
    nad_vec_drop(a);
    nad_vec_drop(b);
    nad_al_arena_drop(arena);

    return rc;
}

int main() {
    if (show_exchange() != 0) {
        return 1;
    }

    /// [build]
    // the handle comes back through 'out', and the status cannot be ignored
    nad_Al *al = nad_al_default();

    nad_Vec *v = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_VEC_OF(int32_t, al, &v, 5, 3, 1))) {
        return 1;
    }

    printf("%zu of %zu\n", nad_vec_len(v), nad_vec_cap(v)); // 3 of 3
    /// [build]

    /// [grow]
    // the block doubles when it fills, so a run of pushes costs O(1) amortized per elem.
    // Every op that may reallocate returns a status, and C has no defer: once the vec is
    // held, the failure path jumps to a common exit instead of returning early
    int rc = 1;

    if (NAD_STATUS_IS_ERR(NAD_VEC_PUSH(int32_t, v, 4))) {
        goto out;
    }
    printf("%zu of %zu\n", nad_vec_len(v), nad_vec_cap(v)); // 4 of 6

    // a pop moves the length and leaves the room alone
    nad_vec_pop(v);
    printf("%zu of %zu\n", nad_vec_len(v), nad_vec_cap(v)); // 3 of 6

    // shrink_to_fit is how the room goes back, and it can fail like any other resize
    if (NAD_STATUS_IS_ERR(nad_vec_shrink_to_fit(v))) {
        goto out;
    }
    printf("%zu of %zu\n", nad_vec_len(v), nad_vec_cap(v)); // 3 of 3
    /// [grow]

    /// [bulk]
    // extend takes the room once for the whole run rather than once per elem, and
    // insert_span moves the tail once where a loop of insert would move it per elem
    if (NAD_STATUS_IS_ERR(NAD_VEC_EXTEND(int32_t, v, 4, 2))) {
        goto out;
    }
    nad_vec_print(v, nad_fprint_i32); // [5, 3, 1, 4, 2]

    if (NAD_STATUS_IS_ERR(NAD_VEC_INSERT_SPAN(int32_t, v, 1, 9, 9))) {
        goto out;
    }
    nad_vec_print(v, nad_fprint_i32); // [5, 9, 9, 3, 1, 4, 2]

    // and the way back out, which allocates nothing and so cannot fail
    nad_vec_remove_range(v, 1, 2);
    nad_vec_print(v, nad_fprint_i32); // [5, 3, 1, 4, 2]
    /// [bulk]

    /// [algo]
    // the bridge to algo runs both ways. Out: a writable view, and algo rearranges the
    // elems in place — a vec keeps no order of its own to protect
    nad_span_sort(nad_vec_to_span_mut(v), nad_cmp_i32);
    nad_vec_print(v, nad_fprint_i32); // [1, 2, 3, 4, 5]

    // back: an algorithm that drops elems packs the kept ones to the front and returns
    // how many there are, and the vec adopts that length
    const size_t kept = nad_span_remove(nad_vec_to_span_mut(v), &(int32_t){3}, nad_eq_i32);
    if (NAD_STATUS_IS_ERR(nad_vec_resize(v, kept))) {
        goto out;
    }
    nad_vec_print(v, nad_fprint_i32); // [1, 2, 4, 5]

    printf(
        "%" PRId32 " .. %" PRId32 "\n", *NAD_VEC_FRONT_AS(int32_t, v),
        *NAD_VEC_BACK_AS(int32_t, v)
    ); // 1 .. 5

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    nad_vec_drop(v);
    return rc;
    /// [algo]
}
