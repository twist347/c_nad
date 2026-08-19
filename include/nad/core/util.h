#pragma once

/* ========== macro ========== */

#define NAD_STRINGIFY_(x)   #x

#define NAD_STRINGIFY(x)    NAD_STRINGIFY_(x)

#define NAD_UNUSED(val)    ((void) (val))

#define NAD_SWAP(a, b)                                        \
    do {                                                      \
        typeof_unqual(a) *nad_swap_pa_ = &(a);                \
        typeof_unqual(a) *nad_swap_pb_ = &(b);                \
        const typeof_unqual(a) nad_swap_tmp_ = *nad_swap_pa_; \
        *nad_swap_pa_ = *nad_swap_pb_;                        \
        *nad_swap_pb_ = nad_swap_tmp_;                        \
    } while (0)
