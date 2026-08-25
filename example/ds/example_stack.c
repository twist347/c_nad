// for @snippet

#include "nad/algo/search.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/ds/stack.h"
#include "nad/ds/vec.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the elems are pushed in the order given, so the FIRST is at the bottom and the last
    // is on top
    nad_Al *al = nad_al_default();

    nad_Stack *s = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_STACK_OF(int32_t, al, &s, 1, 2, 3))) {
        return 1;
    }

    nad_stack_print(s, nad_fprint_i32); // [1, 2, 3]
    printf("%" PRId32 "\n", *NAD_STACK_TOP_AS(int32_t, s)); // 3
    /// [build]

    /// [lifo]
    // one end, and only one: there is no get by index, no insert, no remove and no way to
    // reach the bottom. That narrowing is the whole type — the vec underneath could do all
    // of it
    int rc = 1;
    nad_Vec *v = nullptr; // taken from the stack further down, and named here so the exit sees it

    if (NAD_STATUS_IS_ERR(NAD_STACK_PUSH(int32_t, s, 4))) {
        goto out;
    }
    printf("%" PRId32 "\n", *NAD_STACK_TOP_AS(int32_t, s)); // 4

    // the top is writable: what makes a stack a stack is where elems enter and leave, not
    // what they hold
    *NAD_STACK_TOP_MUT_AS(int32_t, s) = 9;
    nad_stack_print(s, nad_fprint_i32); // [1, 2, 3, 9]

    // read the elem before dropping it — a pop that returned it would have nowhere to put
    // the bytes
    while (nad_stack_len(s) > 0) {
        printf("%" PRId32 " ", *NAD_STACK_TOP_AS(int32_t, s));
        nad_stack_pop(s);
    }
    putchar('\n'); // 9 3 2 1
    /// [lifo]

    /// [read]
    // the arrival order is the stack's to keep, so the bridge to algo is read only: a
    // view to search, count or fold, and no mutable one to sort through
    if (NAD_STATUS_IS_ERR(NAD_STACK_PUSH(int32_t, s, 7))) {
        goto out;
    }
    if (NAD_STATUS_IS_ERR(NAD_STACK_PUSH(int32_t, s, 8))) {
        goto out;
    }

    const nad_Span view = nad_stack_to_span(s); // bottom to top, so the top is LAST

    size_t idx;
    if (nad_span_find(view, &(int32_t){8}, nad_eq_i32, &idx)) {
        printf("8 sits %zu from the bottom of %zu\n", idx, view.len); // 8 sits 1 from the bottom of 2
    }

    // the view borrows the buffer, so the next push may leave it dangling
    if (NAD_STATUS_IS_ERR(NAD_STACK_PUSH(int32_t, s, 5))) {
        goto out;
    }
    nad_stack_print(s, nad_fprint_i32); // [7, 8, 5]
    /// [read]

    /// [into]
    // the vec was under the stack all along, so taking it costs nothing: the same block,
    // the same capacity, the same allocator, not one elem copied. It CONSUMES the stack —
    // the header goes back to the allocator and the handle must not be used again
    v = nad_stack_into_vec(s);
    s = nullptr;

    // and the narrow interface goes with it: an index was refused while the stack existed,
    // and the stack no longer does
    nad_vec_print(v, nad_fprint_i32); // [7, 8, 5]
    printf("%" PRId32 "\n", *NAD_VEC_GET_AS(int32_t, v, 0)); // 7

    rc = 0;
out:
    // a null handle is a no-op, so this drops whichever of the two is still alive
    nad_vec_drop(v);
    nad_stack_drop(s);
    return rc;
    /// [into]
}
