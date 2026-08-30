// for @snippet

#include "nad/alloc/aligned.h"
#include "nad/alloc/default.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
    alignas(64) double v[8];
} Lane;

int main() {
    /// [build]
    nad_Al *aligned = nad_al_aligned_new(nad_al_default(), alignof(Lane));
    if (!aligned) {
        return 1;
    }

    int rc = 1;
    Lane *lanes = NAD_ALLOC(Lane, aligned, 16);
    if (!lanes) {
        goto drop;
    }

    printf("%d\n", (int) ((uintptr_t) lanes % alignof(Lane) == 0)); // 1 - malloc alone would not promise it

    // the growth goes through alloc and copy, so the new block is aligned too
    Lane *grown = NAD_REALLOC(Lane, aligned, lanes, 16, 64);
    if (!grown) {
        NAD_DEALLOC(Lane, aligned, lanes, 16);
        goto drop;
    }

    printf("%d\n", (int) ((uintptr_t) grown % alignof(Lane) == 0)); // 1

    NAD_DEALLOC(Lane, aligned, grown, 64);
    /// [build]

    rc = 0;

drop:
    nad_al_aligned_drop(aligned);

    return rc;
}
