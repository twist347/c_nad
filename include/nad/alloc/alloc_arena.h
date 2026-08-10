#pragma once

#include "nad/core/export.h"
#include "nad/alloc/alloc.h"

#include <stddef.h>

[[nodiscard]] NAD_API
nad_Al *nad_al_arena_new(nad_Al *parent, size_t cap);

NAD_API
void nad_al_arena_drop(nad_Al *al);

NAD_API
void nad_al_arena_reset(nad_Al *al);

typedef struct {
    size_t cap;
    size_t used;
    size_t available;
} nad_AlArenaStats;

[[nodiscard]] NAD_API
nad_AlArenaStats nad_al_arena_stats(const nad_Al *al);
