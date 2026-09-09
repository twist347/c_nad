// for @snippet

#include "terse/algo/search.h"
#include "terse/alloc/default.h"
#include "terse/core/cmp.h"
#include "terse/core/print.h"
#include "terse/core/span.h"
#include "terse/ds/deque.h"
#include "terse/ds/queue.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    /// [build]
    // the elems join in the order given, so the first one in is the first one served
    trs_Al *al = trs_al_default();

    trs_Queue *q = nullptr;
    if (TRS_STATUS_IS_ERR(TRS_QUEUE_OF(int32_t, al, &q, 1, 2, 3))) {
        return 1;
    }

    trs_queue_print(q, trs_fprint_i32); // [1, 2, 3]
    printf(
        "%" PRId32 " .. %" PRId32 "\n", *TRS_QUEUE_FRONT_AS(int32_t, q),
        *TRS_QUEUE_BACK_AS(int32_t, q)
    ); // 1 .. 3
    /// [build]

    /// [fifo]
    // two ends, each with one job: elems join at the back and leave from the front, both
    // in O(1) amortized. There is no get by index, no insert, no remove, no push_front and
    // no pop_back — the deque underneath could do all of it
    int rc = 1;
    trs_Deque *d = nullptr; // taken from the queue further down, and named here for the exit

    if (TRS_STATUS_IS_ERR(TRS_QUEUE_PUSH(int32_t, q, 4))) {
        goto out;
    }
    trs_queue_print(q, trs_fprint_i32); // [1, 2, 3, 4]

    // both ends are writable: what makes a queue a queue is where elems enter and leave,
    // not what they hold
    *TRS_QUEUE_FRONT_MUT_AS(int32_t, q) = 9;

    // read the elem before dropping it — a pop that returned it would have nowhere to put
    // the bytes
    printf("%" PRId32 " ", *TRS_QUEUE_FRONT_AS(int32_t, q));
    trs_queue_pop(q);
    printf("%" PRId32 "\n", *TRS_QUEUE_FRONT_AS(int32_t, q)); // 9 2
    /// [fifo]

    /// [read]
    // the elems may wrap the ring underneath, so there is no view to hand over: the whole
    // bridge to algo is one copy out into a buffer exactly as long as the queue
    int32_t buf[8];
    const trs_SpanMut out_span = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, trs_queue_len(q));

    trs_queue_copy_to_span(q, out_span);
    const trs_Span view = trs_span_mut_to_span(out_span);

    printf("%zu waiting, %zu of them 3\n", view.len,
           trs_span_count(view, &(int32_t){3}, trs_eq_i32)); // 3 waiting, 1 of them 3

    // and it runs one way only: sorting the copy would say nothing about the queue, so
    // there is no copy_from_span to write one back
    trs_queue_print(q, trs_fprint_i32); // [2, 3, 4]
    /// [read]

    /// [into]
    // the deque was under the queue all along, ring and all, so taking it costs nothing.
    // It CONSUMES the queue: the header goes back and the handle must not be used again
    d = trs_queue_into_deque(q);
    q = nullptr;

    // and the narrow interface goes with it: a queue refuses a push at the front, the
    // deque underneath never did
    if (TRS_STATUS_IS_ERR(TRS_DEQUE_PUSH_FRONT(int32_t, d, 1))) {
        goto out;
    }
    trs_deque_print(d, trs_fprint_i32); // [1, 2, 3, 4]

    rc = 0;
out:
    // a null handle is a no-op, so this drops whichever of the two is still alive
    trs_deque_drop(d);
    trs_queue_drop(q);
    return rc;
    /// [into]
}
