// for @snippet

#include "tda/alloc/aligned.h"
#include "tda/alloc/default.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
    alignas(64) double v[8];
} Lane;

int main() {
    /// [build]
    tda_Al *aligned = tda_al_aligned_new(tda_al_default(), alignof(Lane));
    if (!aligned) {
        return 1;
    }

    int rc = 1;
    Lane *lanes = TDA_ALLOC(Lane, aligned, 16);
    if (!lanes) {
        goto drop;
    }

    printf("%d\n", (int) ((uintptr_t) lanes % alignof(Lane) == 0)); // 1 - malloc alone would not promise it

    // the growth goes through alloc and copy, so the new block is aligned too
    Lane *grown = TDA_REALLOC(Lane, aligned, lanes, 16, 64);
    if (!grown) {
        TDA_DEALLOC(Lane, aligned, lanes, 16);
        goto drop;
    }

    printf("%d\n", (int) ((uintptr_t) grown % alignof(Lane) == 0)); // 1

    TDA_DEALLOC(Lane, aligned, grown, 64);
    /// [build]

    rc = 0;

drop:
    tda_al_aligned_drop(aligned);

    return rc;
}
