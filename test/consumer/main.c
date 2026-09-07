#include "nad/nad.h"

#include <stdio.h>

int main() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024); // alloc
    if (!arena) {
        return 1;
    }

    int rc = 1;
    nad_Vec *vec = nullptr; // ds
    if (NAD_STATUS_IS_ERR(NAD_VEC_OF(int32_t, arena, &vec, 5, 3, 1, 4, 2))) {
        goto out;
    }

    nad_span_sort(nad_vec_to_span_mut(vec), nad_cmp_i32); // algo, core/cmp

    size_t idx; // core/span
    if (!nad_span_binary_search(nad_vec_to_span(vec), &(int32_t){4}, nad_cmp_i32, &idx)) {
        goto out;
    }

    nad_Rng rng = nad_rng_from_seed(1); // core/rng
    nad_span_shuffle(nad_vec_to_span_mut(vec), &rng);

    printf("consumer ok: len %zu, 4 sorted at %zu\n", nad_vec_len(vec), idx);
    rc = 0;

out:
    nad_vec_drop(vec);
    nad_al_arena_drop(arena);
    return rc;
}
