#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"

#include <stddef.h>

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Al *nad_al_arena_new(nad_Al *parent, size_t cap);

NAD_API
void nad_al_arena_drop(nad_Al *al);

/* ========== mods ========== */

NAD_API
void nad_al_arena_reset(nad_Al *al);

/* ========== stats ========== */

typedef struct {
    size_t cap;
    size_t used;
    size_t available;
} nad_AlArenaStats;

[[nodiscard]] NAD_API
nad_AlArenaStats nad_al_arena_stats(const nad_Al *al);
