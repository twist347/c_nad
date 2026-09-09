// for @snippet

#include "terse/alloc/default.h"
#include "terse/core/print.h"
#include "terse/core/status.h"
#include "terse/ds/arr.h"

#include <stdio.h>

/// [propagate]
// a fallible op returns the status and writes its result through a trailing 'out'
[[nodiscard]]
static trs_Status two_arrs(trs_Al *al, trs_Arr **out) {
    trs_Arr *a = nullptr;
    trs_Status st = TRS_ARR_OF(int32_t, al, &a, 5, 3, 1);
    if (TRS_STATUS_IS_ERR(st)) {
        return st; // nothing is held yet, so the error goes straight back
    }

    trs_Arr *b = nullptr;
    st = trs_arr_copy(a, &b);
    if (TRS_STATUS_IS_ERR(st)) {
        goto drop_a; // 'a' is held, and C has no defer: the failure path is a goto
    }
    trs_arr_drop(b);

    *out = a; // 'out' is written on the one path that succeeded, and only there
    return TRS_STATUS_OK;

drop_a:
    trs_arr_drop(a);
    return st;
}

/// [propagate]

int main() {
    /// [report]
    trs_Arr *a = nullptr;
    const trs_Status st = two_arrs(trs_al_default(), &a);

    printf("%s\n", trs_status_to_str(st)); // TRS_STATUS_OK
    if (TRS_STATUS_IS_ERR(st)) {
        return 1;
    }

    trs_arr_print(a, trs_fprint_i32); // [5, 3, 1]
    trs_arr_drop(a);
    /// [report]

    return 0;
}
