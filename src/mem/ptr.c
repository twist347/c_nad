#include "nad/mem/ptr.h"

#include <assert.h>
#include <stdint.h>

/* ========== byte-level pointer arithmetic ========== */

const char *nad_byte_offset(const void *base, size_t stride, size_t n) {
    assert(base);
    assert(stride > 0);

    return (const char *) base + n * stride;
}

char *nad_byte_offset_mut(void *base, size_t stride, size_t n) {
    assert(base);
    assert(stride > 0);

    return (char *) base + n * stride;
}

ptrdiff_t nad_byte_diff(const void *a, const void *b) {
    assert(a);
    assert(b);

    return (const char *) a - (const char *) b;
}

size_t nad_ptr_distance(const void *a, const void *b, size_t stride) {
    assert(a);
    assert(b);
    assert(stride > 0);
    assert((const char *) a >= (const char *) b);

    const size_t diff = nad_byte_diff(a, b);
    assert(diff % stride == 0);

    return diff / stride;
}

/* ========== alignment ========== */

size_t nad_align_up(size_t val, size_t alignment) {
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0); // power of 2

    return (val + alignment - 1) & ~(alignment - 1);
}

size_t nad_align_down(size_t val, size_t alignment) {
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0); // power of 2

    return val & ~(alignment - 1);
}

bool nad_ptr_is_aligned(const void *ptr, size_t alignment) {
    assert(ptr);
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0); // power of 2

    return ((uintptr_t) ptr & (alignment - 1)) == 0;
}
