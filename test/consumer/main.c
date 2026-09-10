#include "trs/dsa.h"

#include <stdio.h>

int main() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024); // alloc
    if (!arena) {
        return 1;
    }

    int rc = 1;
    trs_Vec *vec = nullptr; // ds
    if (TRS_STATUS_IS_ERR(TRS_VEC_OF(int32_t, arena, &vec, 5, 3, 1, 4, 2))) {
        goto out;
    }

    trs_span_sort(trs_vec_to_span_mut(vec), trs_cmp_i32); // algo, core/cmp

    size_t idx; // core/span
    if (!trs_span_binary_search(trs_vec_to_span(vec), &(int32_t){4}, trs_cmp_i32, &idx)) {
        goto out;
    }

    trs_Rng rng = trs_rng_from_seed(1); // core/rng
    trs_span_shuffle(trs_vec_to_span_mut(vec), &rng);

    printf("consumer ok: len %zu, 4 sorted at %zu\n", trs_vec_len(vec), idx);
    rc = 0;

out:
    trs_vec_drop(vec);
    trs_al_arena_drop(arena);
    return rc;
}
