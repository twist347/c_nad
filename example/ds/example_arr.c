// for @snippet

#include "nad/algo/search.h"
#include "nad/algo/sort.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/ds/arr.h"

#include <inttypes.h>
#include <stdio.h>

int main() {
    /// [build]
    // the handle comes back through 'out', and the status cannot be ignored
    nad_Al *al = nad_al_default();

    nad_Arr *a = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_ARR_OF(int32_t, al, &a, 5, 3, 1, 4, 2))) {
        return 1;
    }
    /// [build]

    /// [algo]
    // an arr has no order of its own to protect, so algo rearranges the elems in place
    nad_span_sort(nad_arr_to_span_mut(a), nad_cmp_i32);

    size_t idx;
    if (nad_span_binary_search(nad_arr_to_span(a), &(int32_t){4}, nad_cmp_i32, &idx)) {
        printf("4 is at %zu\n", idx); // 4 is at 3
    }
    /// [algo]

    /// [access]
    NAD_ARR_SET(int32_t, a, 0, 0);
    printf("%" PRId32 " .. %" PRId32 " over %zu elems\n", *NAD_ARR_FIRST_AS(int32_t, a),
           *NAD_ARR_LAST_AS(int32_t, a), nad_arr_len(a)); // 0 .. 5 over 5 elems
    /// [access]

    /// [copy]
    // every op that allocates can fail, and C has no defer: once a resource is held, the
    // failure path jumps to a common exit instead of returning early
    nad_Arr *copy = nullptr;
    nad_Arr *shorter = nullptr;
    int rc = 1;

    // one way to copy: a fresh arr with the same elems, on the same allocator as 'a'
    if (NAD_STATUS_IS_ERR(nad_arr_copy(a, &copy))) {
        goto out;
    }
    nad_arr_print(copy, nad_fprint_i32); // [0, 2, 3, 4, 5]

    // the other: an arr that already exists is overwritten, and its block is resized to
    // whatever the source needs — here from two elems to five
    if (NAD_STATUS_IS_ERR(NAD_ARR_NEW_LEN(int32_t, 2, al, &shorter))) {
        goto out;
    }
    nad_arr_print(shorter, nad_fprint_i32); // [0, 0] — new_len zeroes the block

    if (NAD_STATUS_IS_ERR(nad_arr_copy_assign(a, shorter))) {
        goto out;
    }
    nad_arr_print(shorter, nad_fprint_i32); // [0, 2, 3, 4, 5]

    // and the one operation that moves a block: the two are exchanged whole, lengths and
    // all, so a pointer into either of them now points into the other
    if (NAD_STATUS_IS_ERR(nad_arr_swap(a, shorter))) {
        goto out;
    }

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    nad_arr_drop(shorter);
    nad_arr_drop(copy);
    nad_arr_drop(a);
    return rc;
    /// [copy]
}
