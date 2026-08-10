#pragma once

#include "nad/core/export.h"

#include <stddef.h>

typedef struct {
    void *ctx;

    void *(*alloc)(void *ctx, size_t size);
    void *(*calloc)(void *ctx, size_t num, size_t size);
    void *(*realloc)(void *ctx, void *ptr, size_t old_size, size_t new_size);
    void (*dealloc)(void *ctx, void *ptr, size_t size);
} nad_Allocator;

/* ========== wrappers ========== */

// Returns size bytes, or nullptr. nullptr means either size == 0 (nothing requested —
// defined behaviour, not an error) or the allocation failed. With size > 0, nullptr is
// unambiguously out-of-memory.
[[nodiscard]] NAD_API
void *nad_alloc(nad_Allocator *al, size_t size);

// Returns num * size zero-initialized bytes, or nullptr. nullptr means either num or size
// is 0, num * size overflows size_t, or the allocation failed. With a non-zero,
// non-overflowing request, nullptr is unambiguously out-of-memory.
[[nodiscard]] NAD_API
void *nad_calloc(nad_Allocator *al, size_t num, size_t size);

// Resizes ptr (of old_size bytes) to new_size, or nullptr. new_size == 0 frees ptr and
// returns nullptr (defined behaviour, not an error). Otherwise nullptr means the
// allocation failed and ptr is left untouched — assign to a temporary, not over ptr.
[[nodiscard]] NAD_API
void *nad_realloc(nad_Allocator *al, void *ptr, size_t old_size, size_t new_size);

// Frees ptr; no-op if ptr is nullptr. size must match the original allocation.
NAD_API
void nad_dealloc(nad_Allocator *al, void *ptr, size_t size);

/* ========== macros ========== */

#define NAD_ALLOC(T, al, count) \
    ((T*) nad_alloc((al), (count) * sizeof(T)))

#define NAD_CALLOC(T, al, count) \
    ((T*) nad_calloc((al), (count), sizeof(T)))

#define NAD_REALLOC(T, al, ptr, old_count, new_count) \
    ((T*) nad_realloc((al), (ptr), (old_count) * sizeof(T), (new_count) * sizeof(T)))

#define NAD_DEALLOC(T, al, ptr, count) \
    nad_dealloc((al), (ptr), (count) * sizeof(T))
