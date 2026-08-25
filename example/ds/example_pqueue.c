// for @snippet

#include "nad/algo/heap.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/ds/pqueue.h"
#include "nad/ds/vec.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the elems arrive in any order and are heapified in one pass, which is cheaper than
    // pushing them one at a time
    nad_Al *al = nad_al_default();

    nad_PQueue *q = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_PQUEUE_OF(int32_t, nad_cmp_i32, al, &q, 3, 1, 4, 1, 5))) {
        return 1;
    }

    // what a print shows is heap order, not sorted order: only the front is in its final
    // place
    nad_pqueue_print(q, nad_fprint_i32); // [5, 3, 4, 1, 1]
    printf("%" PRId32 "\n", *NAD_PQUEUE_TOP_AS(int32_t, q)); // 5
    /// [build]

    /// [serve]
    // a push finds its place in O(log n), and the front is whatever is greatest now
    int rc = 1;
    nad_Vec *v = nullptr; // taken from a queue further down, and named here for the exit

    if (NAD_STATUS_IS_ERR(NAD_PQUEUE_PUSH(int32_t, q, 9))) {
        goto out;
    }
    printf("%" PRId32 "\n", *NAD_PQUEUE_TOP_AS(int32_t, q)); // 9

    // draining the queue is how its elems come out sorted; read each one before dropping
    // it, since a pop that returned it would have nowhere to put the bytes
    while (nad_pqueue_len(q) > 0) {
        printf("%" PRId32 " ", *NAD_PQUEUE_TOP_AS(int32_t, q));
        nad_pqueue_pop(q);
    }
    putchar('\n'); // 9 5 4 3 1 1
    /// [serve]

    /// [order]
    // the comparator is a property of the queue, not of a call, so a min-queue is this
    // same type built with a descending one — there is no second type
    nad_PQueue *least = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_PQUEUE_OF(int32_t, nad_cmp_desc_i32, al, &least, 3, 1, 4))) {
        goto out;
    }
    printf("%" PRId32 "\n", *NAD_PQUEUE_TOP_AS(int32_t, least)); // 1

    // nothing here hands out a writable elem — no top_mut, no get, no mutable view — since
    // a write through one would break the heap with no way for the queue to notice
    const nad_Span view = nad_pqueue_to_span(least);
    printf("%zu elems, the least of them %" PRId32 "\n", view.len,
           *NAD_SPAN_GET_AS(int32_t, view, 0)); // 3 elems, the least of them 1
    /// [order]

    /// [into]
    // the vec was under the queue all along, so taking it costs nothing and CONSUMES the
    // queue. The elems come out in heap order, and sort_heap finishes the job in place:
    // that is the second half of heapsort, with no allocation and no draining loop
    v = nad_pqueue_into_vec(least);

    // the comparator does NOT travel with the elems — it was the queue's, not theirs — so
    // the same one has to be named again. This heap was built descending, so sorting it
    // under the same order puts the greatest first
    nad_span_sort_heap(nad_vec_to_span_mut(v), nad_cmp_desc_i32);
    nad_vec_print(v, nad_fprint_i32); // [4, 3, 1]

    rc = 0;
out:
    // a null handle is a no-op, so this exit is safe from anywhere above
    nad_vec_drop(v);
    nad_pqueue_drop(q);
    return rc;
    /// [into]
}
