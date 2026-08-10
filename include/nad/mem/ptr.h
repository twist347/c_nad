#pragma once

#include "nad/core/export.h"

#include <stddef.h>

/* ========== byte-level pointer arithmetic ========== */

[[nodiscard]] NAD_API
const char *nad_byte_offset(const void *base, size_t stride, size_t n);

[[nodiscard]] NAD_API
char *nad_byte_offset_mut(void *base, size_t stride, size_t n);

[[nodiscard]] NAD_API
ptrdiff_t nad_byte_diff(const void *a, const void *b);

[[nodiscard]] NAD_API
size_t nad_ptr_distance(const void *a, const void *b, size_t stride);

/* ========== alignment ========== */

[[nodiscard]] NAD_API
size_t nad_align_up(size_t val, size_t alignment);

[[nodiscard]] NAD_API
size_t nad_align_down(size_t val, size_t alignment);

[[nodiscard]] NAD_API
bool nad_ptr_is_aligned(const void *ptr, size_t alignment);
