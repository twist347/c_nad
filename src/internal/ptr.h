#pragma once

#include "nad/core/util.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* ========== byte-level pointer arithmetic ========== */

[[nodiscard]]
static inline const unsigned char *nad_byte_offset(const void *base, size_t stride, size_t n) {
    assert(base || n == 0);
    assert(stride > 0);

    return (const unsigned char *) base + stride * n;
}

[[nodiscard]]
static inline unsigned char *nad_byte_offset_mut(void *base, size_t stride, size_t n) {
    assert(base || n == 0);
    assert(stride > 0);

    return (unsigned char *) base + stride * n;
}

[[nodiscard]]
static inline ptrdiff_t nad_byte_diff(const void *a, const void *b) {
    assert(a);
    assert(b);

    return (const unsigned char *) a - (const unsigned char *) b;
}

[[nodiscard]]
static inline size_t nad_ptr_distance(const void *a, const void *b, size_t stride) {
    assert(a);
    assert(b);
    assert(stride > 0);

    const ptrdiff_t diff = nad_byte_diff(a, b);
    assert(diff >= 0);
    assert((size_t) diff % stride == 0);

    return (size_t) diff / stride;
}

/* ========== alignment ========== */

/// what every allocator here hands out when nothing wider is asked for: the alignment
/// malloc itself promises, and the floor under alloc/aligned's parameter
static constexpr size_t NAD_DEFAULT_ALIGNMENT = alignof(max_align_t);

[[nodiscard]]
static inline size_t nad_align_up(size_t val, size_t alignment) {
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0);
    assert(val <= SIZE_MAX - (alignment - 1));

    return (val + (alignment - 1)) & ~(alignment - 1);
}

[[nodiscard]]
static inline size_t nad_align_down(size_t val, size_t alignment) {
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0);

    return val & ~(alignment - 1);
}

/// the first address at or after 'ptr' that is aligned to 'alignment'. The mask is built
/// in uintptr_t: spelled over size_t it would be a narrower type wherever the two differ,
/// and the address would be cut down instead of rounded up
[[nodiscard]]
static inline void *nad_ptr_align_up(void *ptr, size_t alignment) {
    assert(ptr);
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0);

    const uintptr_t addr = (uintptr_t) ptr;
    assert(addr <= UINTPTR_MAX - (alignment - 1));

    return (void *) ((addr + ((uintptr_t) alignment - 1)) & ~((uintptr_t) alignment - 1));
}

[[nodiscard]]
static inline bool nad_ptr_is_aligned(const void *ptr, size_t alignment) {
    assert(ptr);
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0);

    return ((uintptr_t) ptr & (alignment - 1)) == 0;
}

static inline void nad_bytes_swap(void *a, void *b, size_t n) {
    assert(a);
    assert(b);

    unsigned char *pa = a;
    unsigned char *pb = b;
    for (size_t i = 0; i < n; ++i) {
        NAD_SWAP(pa[i], pb[i]);
    }
}
