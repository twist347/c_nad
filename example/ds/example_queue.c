// for @snippet

#include "nad/algo/search.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/core/span.h"
#include "nad/ds/deque.h"
#include "nad/ds/queue.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the elems join in the order given, so the first one in is the first one served
    nad_Al *al = nad_al_default();

    nad_Queue *q = nullptr;
    if (NAD_STATUS_IS_ERR(NAD_QUEUE_OF(int32_t, al, &q, 1, 2, 3))) {
        return 1;
    }

    nad_queue_print(q, nad_fprint_i32); // [1, 2, 3]
    printf(
        "%" PRId32 " .. %" PRId32 "\n", *NAD_QUEUE_FRONT_AS(int32_t, q),
        *NAD_QUEUE_BACK_AS(int32_t, q)
    ); // 1 .. 3
    /// [build]

    /// [fifo]
    // two ends, each with one job: elems join at the back and leave from the front, both
    // in O(1) amortized. There is no get by index, no insert, no remove, no push_front and
    // no pop_back — the deque underneath could do all of it
    int rc = 1;
    nad_Deque *d = nullptr; // taken from the queue further down, and named here for the exit

    if (NAD_STATUS_IS_ERR(NAD_QUEUE_PUSH(int32_t, q, 4))) {
        goto out;
    }
    nad_queue_print(q, nad_fprint_i32); // [1, 2, 3, 4]

    // both ends are writable: what makes a queue a queue is where elems enter and leave,
    // not what they hold
    *NAD_QUEUE_FRONT_MUT_AS(int32_t, q) = 9;

    // read the elem before dropping it — a pop that returned it would have nowhere to put
    // the bytes
    printf("%" PRId32 " ", *NAD_QUEUE_FRONT_AS(int32_t, q));
    nad_queue_pop(q);
    printf("%" PRId32 "\n", *NAD_QUEUE_FRONT_AS(int32_t, q)); // 9 2
    /// [fifo]

    /// [read]
    // the elems may wrap the ring underneath, so there is no view to hand over: the whole
    // bridge to algo is one copy out into a buffer exactly as long as the queue
    int32_t buf[8];
    const nad_SpanMut out_span = NAD_SPAN_NEW_MUT(int32_t, buf, nad_queue_len(q));

    nad_queue_copy_to_span(q, out_span);
    const nad_Span view = nad_span_mut_to_span(out_span);

    printf("%zu waiting, %zu of them 3\n", view.len,
           nad_span_count(view, &(int32_t){3}, nad_eq_i32)); // 3 waiting, 1 of them 3

    // and it runs one way only: sorting the copy would say nothing about the queue, so
    // there is no copy_from_span to write one back
    nad_queue_print(q, nad_fprint_i32); // [2, 3, 4]
    /// [read]

    /// [into]
    // the deque was under the queue all along, ring and all, so taking it costs nothing.
    // It CONSUMES the queue: the header goes back and the handle must not be used again
    d = nad_queue_into_deque(q);
    q = nullptr;

    // and the narrow interface goes with it: a queue refuses a push at the front, the
    // deque underneath never did
    if (NAD_STATUS_IS_ERR(NAD_DEQUE_PUSH_FRONT(int32_t, d, 1))) {
        goto out;
    }
    nad_deque_print(d, nad_fprint_i32); // [1, 2, 3, 4]

    rc = 0;
out:
    // a null handle is a no-op, so this drops whichever of the two is still alive
    nad_deque_drop(d);
    nad_queue_drop(q);
    return rc;
    /// [into]
}
