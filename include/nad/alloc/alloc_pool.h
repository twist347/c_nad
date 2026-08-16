#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"

#include <stddef.h>

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Al *nad_al_pool_new(nad_Al *parent, size_t block_size, size_t block_count);

NAD_API
void nad_al_pool_drop(nad_Al *al);

/* ========== mods ========== */

NAD_API
void nad_al_pool_reset(nad_Al *al);

/* ========== stats ========== */

typedef struct {
    size_t block_size;
    size_t block_count;
    size_t used;
    size_t free;
} nad_AlPoolStats;

[[nodiscard]] NAD_API
nad_AlPoolStats nad_al_pool_stats(const nad_Al *al);
