// for @snippet

#include "trs/algo/search.h"
#include "trs/alloc/default.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"
#include "trs/ds/stack.h"
#include "trs/ds/vec.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the elems are pushed in the order given, so the FIRST is at the bottom and the last
    // is on top
    trs_Al *al = trs_al_default();

    trs_Stack *s = nullptr;
    if (TRS_STATUS_IS_ERR(TRS_STACK_OF(int32_t, al, &s, 1, 2, 3))) {
        return 1;
    }

    trs_stack_print(s, trs_fprint_i32); // [1, 2, 3]
    printf("%" PRId32 "\n", *TRS_STACK_TOP_AS(int32_t, s)); // 3
    /// [build]

    /// [lifo]
    // one end, and only one: there is no get by index, no insert, no remove and no way to
    // reach the bottom. That narrowing is the whole type — the vec underneath could do all
    // of it
    int rc = 1;
    trs_Vec *v = nullptr; // taken from the stack further down, and named here so the exit sees it

    if (TRS_STATUS_IS_ERR(TRS_STACK_PUSH(int32_t, s, 4))) {
        goto out;
    }
    printf("%" PRId32 "\n", *TRS_STACK_TOP_AS(int32_t, s)); // 4

    // the top is writable: what makes a stack a stack is where elems enter and leave, not
    // what they hold
    *TRS_STACK_TOP_MUT_AS(int32_t, s) = 9;
    trs_stack_print(s, trs_fprint_i32); // [1, 2, 3, 9]

    // read the elem before dropping it — a pop that returned it would have nowhere to put
    // the bytes
    while (trs_stack_len(s) > 0) {
        printf("%" PRId32 " ", *TRS_STACK_TOP_AS(int32_t, s));
        trs_stack_pop(s);
    }
    putchar('\n'); // 9 3 2 1
    /// [lifo]

    /// [read]
    // the arrival order is the stack's to keep, so the bridge to algo is read only: a
    // view to search, count or fold, and no mutable one to sort through
    if (TRS_STATUS_IS_ERR(TRS_STACK_PUSH(int32_t, s, 7))) {
        goto out;
    }
    if (TRS_STATUS_IS_ERR(TRS_STACK_PUSH(int32_t, s, 8))) {
        goto out;
    }

    const trs_Span view = trs_stack_to_span(s); // bottom to top, so the top is LAST

    size_t idx;
    if (trs_span_find(view, &(int32_t){8}, trs_eq_i32, &idx)) {
        printf("8 sits %zu from the bottom of %zu\n", idx, view.len); // 8 sits 1 from the bottom of 2
    }

    // the view borrows the buffer, so the next push may leave it dangling
    if (TRS_STATUS_IS_ERR(TRS_STACK_PUSH(int32_t, s, 5))) {
        goto out;
    }
    trs_stack_print(s, trs_fprint_i32); // [7, 8, 5]
    /// [read]

    /// [into]
    // the vec was under the stack all along, so taking it costs nothing: the same block,
    // the same capacity, the same allocator, not one elem copied. It CONSUMES the stack —
    // the header goes back to the allocator and the handle must not be used again
    v = trs_stack_into_vec(s);
    s = nullptr;

    // and the narrow interface goes with it: an index was refused while the stack existed,
    // and the stack no longer does
    trs_vec_print(v, trs_fprint_i32); // [7, 8, 5]
    printf("%" PRId32 "\n", *TRS_VEC_GET_AS(int32_t, v, 0)); // 7

    rc = 0;
out:
    // a null handle is a no-op, so this drops whichever of the two is still alive
    trs_vec_drop(v);
    trs_stack_drop(s);
    return rc;
    /// [into]
}
