// for @snippet

#include "terse/core/status.h"
#include "terse/core/util.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/// [unused]
static void on_each(void *ctx, int32_t x) {
    TRS_UNUSED(ctx);

    printf("%" PRId32 "\n", x);
}

/// [unused]

int main() {
    /// [swap]
    int32_t a = 1;
    int32_t b = 2;

    TRS_SWAP(a, b);
    printf("%" PRId32 " %" PRId32 "\n", a, b); // 2 1

    // both operands are addressed before anything moves, so an argument that has a side
    // effect is still evaluated once
    int32_t v[] = {10, 20, 30, 40};
    size_t i = 0;
    size_t j = 3;

    TRS_SWAP(v[i++], v[j--]);
    printf("%" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 ", i=%zu j=%zu\n",
           v[0], v[1], v[2], v[3], i, j); // 40 20 30 10, i=1 j=2

    // through pointers, name the pointees: TRS_SWAP(pa, pb) also typechecks, and it
    // exchanges the two local pointers while leaving 'a' and 'b' exactly where they were
    int32_t *pa = &a;
    int32_t *pb = &b;

    TRS_SWAP(*pa, *pb);
    printf("%" PRId32 " %" PRId32 "\n", a, b); // 1 2 — the pointees went back
    /// [swap]

    /// [stringify]
    // the argument is spelled by what it expands to, not by how it was written
    printf("%s\n", TRS_STRINGIFY(TRS_STATUS_OK)); // TRS_STATUS_OK
    /// [stringify]

    on_each(nullptr, 7);

    return 0;
}
