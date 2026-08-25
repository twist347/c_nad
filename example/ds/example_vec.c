// for @snippet

#include "nad/algo/modify.h"
#include "nad/algo/sort.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/ds/vec.h"

#include <inttypes.h>
#include <stdio.h>

int main() {
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
        "%" PRId32 " .. %" PRId32 "\n", *NAD_VEC_FIRST_AS(int32_t, v),
        *NAD_VEC_LAST_AS(int32_t, v)
    ); // 1 .. 5

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    nad_vec_drop(v);
    return rc;
    /// [algo]
}
