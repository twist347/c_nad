// for @snippet

#include "tda/algo/heap.h"
#include "tda/alloc/default.h"
#include "tda/core/cmp.h"
#include "tda/core/print.h"
#include "tda/ds/pqueue.h"
#include "tda/ds/vec.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the elems arrive in any order and are heapified in one pass, which is cheaper than
    // pushing them one at a time
    tda_Al *al = tda_al_default();

    tda_PQueue *q = nullptr;
    if (TDA_STATUS_IS_ERR(TDA_PQUEUE_OF(int32_t, tda_cmp_i32, al, &q, 3, 1, 4, 1, 5))) {
        return 1;
    }

    // what a print shows is heap order, not sorted order: only the front is in its final
    // place
    tda_pqueue_print(q, tda_fprint_i32); // [5, 3, 4, 1, 1]
    printf("%" PRId32 "\n", *TDA_PQUEUE_TOP_AS(int32_t, q)); // 5
    /// [build]

    /// [serve]
    // a push finds its place in O(log n), and the front is whatever is greatest now
    int rc = 1;
    tda_Vec *v = nullptr; // taken from a queue further down, and named here for the exit

    if (TDA_STATUS_IS_ERR(TDA_PQUEUE_PUSH(int32_t, q, 9))) {
        goto out;
    }
    printf("%" PRId32 "\n", *TDA_PQUEUE_TOP_AS(int32_t, q)); // 9

    // draining the queue is how its elems come out sorted; read each one before dropping
    // it, since a pop that returned it would have nowhere to put the bytes
    while (tda_pqueue_len(q) > 0) {
        printf("%" PRId32 " ", *TDA_PQUEUE_TOP_AS(int32_t, q));
        tda_pqueue_pop(q);
    }
    putchar('\n'); // 9 5 4 3 1 1
    /// [serve]

    /// [order]
    // the comparator is a property of the queue, not of a call, so a min-queue is this
    // same type built with a descending one — there is no second type
    tda_PQueue *least = nullptr;
    if (TDA_STATUS_IS_ERR(TDA_PQUEUE_OF(int32_t, tda_cmp_desc_i32, al, &least, 3, 1, 4))) {
        goto out;
    }
    printf("%" PRId32 "\n", *TDA_PQUEUE_TOP_AS(int32_t, least)); // 1

    // nothing here hands out a writable elem — no top_mut, no get, no mutable view — since
    // a write through one would break the heap with no way for the queue to notice
    const tda_Span view = tda_pqueue_to_span(least);
    printf("%zu elems, the least of them %" PRId32 "\n", view.len,
           *TDA_SPAN_GET_AS(int32_t, view, 0)); // 3 elems, the least of them 1
    /// [order]

    /// [into]
    // the vec was under the queue all along, so taking it costs nothing and CONSUMES the
    // queue. The elems come out in heap order, and sort_heap finishes the job in place:
    // that is the second half of heapsort, with no allocation and no draining loop
    v = tda_pqueue_into_vec(least);

    // the comparator does NOT travel with the elems — it was the queue's, not theirs — so
    // the same one has to be named again. This heap was built descending, so sorting it
    // under the same order puts the greatest first
    tda_span_sort_heap(tda_vec_to_span_mut(v), tda_cmp_desc_i32);
    tda_vec_print(v, tda_fprint_i32); // [4, 3, 1]

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    tda_vec_drop(v);
    tda_pqueue_drop(q);
    return rc;
    /// [into]
}
