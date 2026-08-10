#pragma once

#include "nad/core/export.h"
#include "nad/mem/alloc.h"

#include <stddef.h>

[[nodiscard]] NAD_API
nad_Allocator *nad_allocator_arena_new(size_t cap);

NAD_API
void nad_allocator_arena_drop(nad_Allocator *alloc);

NAD_API
void nad_allocator_arena_reset(nad_Allocator *alloc);

typedef struct {
    size_t cap;
    size_t used;
    size_t available;
} nad_AllocatorArenaStats;

[[nodiscard]] NAD_API
nad_AllocatorArenaStats nad_allocator_arena_stats(const nad_Allocator *alloc);
