#pragma once

// NOTE: unused for now

#include "nad/core/export.h"
#include "nad/core/status.h"

#include <stddef.h>

typedef struct {
    nad_Status (*construct)(void *dst, void *ctx);
    nad_Status (*clone)(void *dst, const void *src, void *ctx);
    nad_Status (*assign)(void *dst, const void *src, void *ctx);
    void (*destroy)(void *elem, void *ctx);
} nad_ElemOps;

/* ========== single elem ========== */

[[nodiscard]] NAD_API
nad_Status nad_elem_construct(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    void *ctx
);

[[nodiscard]] NAD_API
nad_Status nad_elem_clone(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    const void *src,
    void *ctx
);

[[nodiscard]] NAD_API
nad_Status nad_elem_assign(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    const void *src,
    void *ctx
);

NAD_API
void nad_elem_destroy(
    const nad_ElemOps *ops,
    void *elem,
    void *ctx
);

/* ========== n elems ========== */

[[nodiscard]] NAD_API
nad_Status nad_elem_construct_n(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    size_t n,
    void *ctx
);

[[nodiscard]] NAD_API
nad_Status nad_elem_clone_n(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    const void *src,
    size_t n,
    void *ctx
);

NAD_API
void nad_elem_destroy_n(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *base,
    size_t n,
    void *ctx
);
