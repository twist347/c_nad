// for @snippet

#include "nad/alloc/default.h"
#include "nad/core/print.h"
#include "nad/core/status.h"
#include "nad/ds/arr.h"

#include <stdio.h>

/// [propagate]
// a fallible op returns the status and writes its result through a trailing 'out'
[[nodiscard]]
static nad_Status two_arrs(nad_Al *al, nad_Arr **out) {
    nad_Arr *a = nullptr;
    nad_Status st = NAD_ARR_OF(int32_t, al, &a, 5, 3, 1);
    if (NAD_STATUS_IS_ERR(st)) {
        return st; // nothing is held yet, so the error goes straight back
    }

    nad_Arr *b = nullptr;
    st = nad_arr_copy(a, &b);
    if (NAD_STATUS_IS_ERR(st)) {
        goto drop_a; // 'a' is held, and C has no defer: the failure path is a goto
    }
    nad_arr_drop(b);

    *out = a; // 'out' is written on the one path that succeeded, and only there
    return NAD_STATUS_OK;

drop_a:
    nad_arr_drop(a);
    return st;
}

/// [propagate]

int main() {
    /// [report]
    nad_Arr *a = nullptr;
    const nad_Status st = two_arrs(nad_al_default(), &a);

    printf("%s\n", nad_status_to_str(st)); // NAD_STATUS_OK
    if (NAD_STATUS_IS_ERR(st)) {
        return 1;
    }

    nad_arr_print(a, nad_fprint_i32); // [5, 3, 1]
    nad_arr_drop(a);
    /// [report]

    return 0;
}
