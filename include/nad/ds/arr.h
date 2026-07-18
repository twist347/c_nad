#pragma once

#include <stddef.h>

#include "nad/core/export.h"
#include "nad/core/status.h"
#include "nad/mem/alloc.h"

typedef struct nad_Arr nad_Arr;

/* ========== lifetime ========== */

[[nodiscard]] NAD_API nad_Status nad_arr_new(size_t len, size_t elem_size, nad_Allocator *alloc, nad_Arr **out);
NAD_API void nad_arr_drop(nad_Arr *self);

/* ========== copy ========== */

[[nodiscard]] NAD_API nad_Status nad_arr_copy(const nad_Arr *self, nad_Arr **out);
[[nodiscard]] NAD_API nad_Status nad_arr_copy_into(const nad_Arr *self, nad_Arr *other);

/* ========== info ========== */

NAD_API size_t nad_arr_len(const nad_Arr *self);
NAD_API size_t nad_arr_elem_size(const nad_Arr *self);
NAD_API nad_Allocator *nad_arr_alloc(const nad_Arr *self);

/* ========== access ========== */

NAD_API const void *nad_arr_get(const nad_Arr *self, size_t idx);
NAD_API void *nad_arr_get_mut(nad_Arr *self, size_t idx);

NAD_API void nad_arr_set(nad_Arr *self, size_t idx, const void *val);

NAD_API const void *nad_arr_data(const nad_Arr *self);
NAD_API void *nad_arr_data_mut(nad_Arr *self);

/* ========== mods ========== */

NAD_API void nad_arr_fill(nad_Arr *self, const void *val);

NAD_API void nad_arr_swap_elems(nad_Arr *self, size_t i, size_t j);
NAD_API void nad_arr_swap(nad_Arr *self, nad_Arr *other);

/* ========== rels ========== */

NAD_API bool nad_arr_eq(const nad_Arr *self, const nad_Arr *other);

/* ========== macros ========== */

#define NAD_ARR_NEW(T, len, alloc, out) \
    nad_arr_new((len), sizeof(T), (alloc), (out))

#define NAD_ARR_GET_AS(T, self, idx) \
    ((const T *) nad_arr_get((self), (idx)))


#define NAD_ARR_GET_MUT_AS(T, self, idx) \
    ((T *) nad_arr_get_mut((self), (idx)))

#define NAD_ARR_SET(T, self, idx, val) \
    nad_arr_set((self), (idx), &(T){ (val) })

#define NAD_ARR_FOREACH(T, it, self)               \
    for (T *it = nad_arr_data_mut(self),           \
            *nad_it_end_ = it + nad_arr_len(self); \
         it != nad_it_end_;                        \
         ++it)
