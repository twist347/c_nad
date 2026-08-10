#include "nad/core/elem_ops.h"

#include <assert.h>
#include <string.h>

static constexpr size_t ELEM_ASSIGN_TMP_MAX = 256;
static constexpr size_t ELEM_MAX_ALIGN = 32;

static const char *byte_offset(const void *base, size_t stride, size_t n);

static char *byte_offset_mut(void *base, size_t stride, size_t n);

/* ========== single elem ========== */

nad_Status nad_elem_construct(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    void *ctx
) {
    assert(elem_size > 0);
    assert(dst);

    if (!ops || !ops->construct) {
        memset(dst, 0, elem_size);
        return NAD_STATUS_OK;
    }

    return ops->construct(dst, ctx);
}

nad_Status nad_elem_clone(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    const void *src,
    void *ctx
) {
    assert(elem_size > 0);
    assert(dst);
    assert(src);
    assert(!ops || !ops->destroy || !ops->clone);

    if (!ops || !ops->clone) {
        memcpy(dst, src, elem_size);
        return NAD_STATUS_OK;
    }

    return ops->clone(dst, src, ctx);
}

nad_Status nad_elem_assign(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    const void *src,
    void *ctx
) {
    assert(elem_size > 0);
    assert(dst);
    assert(src);

    if (dst == src) {
        return NAD_STATUS_OK;
    }

    if (ops && ops->assign) {
        return ops->assign(dst, src, ctx);
    }

    if (!ops || (!ops->clone && !ops->destroy)) {
        memcpy(dst, src, elem_size);
        return NAD_STATUS_OK;
    }

    if (elem_size > ELEM_ASSIGN_TMP_MAX) {
        return NAD_STATUS_UNSUPPORTED;
    }

    alignas(ELEM_MAX_ALIGN) unsigned char tmp[ELEM_ASSIGN_TMP_MAX];

    const nad_Status st = nad_elem_clone(ops, elem_size, tmp, src, ctx);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    nad_elem_destroy(ops, dst, ctx);
    memcpy(dst, tmp, elem_size);

    return NAD_STATUS_OK;
}

void nad_elem_destroy(
    const nad_ElemOps *ops,
    void *elem,
    void *ctx
) {
    assert(elem);

    if (ops && ops->destroy) {
        ops->destroy(elem, ctx);
    }
}

/* ========== n elems ========== */

nad_Status nad_elem_construct_n(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    size_t n,
    void *ctx
) {
    assert(elem_size > 0);
    assert(n == 0 || dst);

    if (!ops || !ops->construct) {
        if (n > 0) {
            memset(dst, 0, n * elem_size);
        }
        return NAD_STATUS_OK;
    }

    for (size_t i = 0; i < n; ++i) {
        const nad_Status st = ops->construct(byte_offset_mut(dst, elem_size, i), ctx);
        if (NAD_STATUS_IS_ERR(st)) {
            nad_elem_destroy_n(ops, elem_size, dst, i, ctx);
            return st;
        }
    }

    return NAD_STATUS_OK;
}

nad_Status nad_elem_clone_n(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *dst,
    const void *src,
    size_t n,
    void *ctx
) {
    assert(elem_size > 0);
    assert(n == 0 || (dst && src));
    assert(!ops || !ops->destroy || !ops->clone);

    if (!ops || !ops->clone) {
        if (n > 0) {
            memcpy(dst, src, n * elem_size);
        }
        return NAD_STATUS_OK;
    }

    for (size_t i = 0; i < n; ++i) {
        const nad_Status st =
                ops->clone(
                    byte_offset_mut(dst, elem_size, i),
                    byte_offset(src, elem_size, i),
                    ctx
                );
        if (NAD_STATUS_IS_ERR(st)) {
            nad_elem_destroy_n(ops, elem_size, dst, i, ctx);
            return st;
        }
    }

    return NAD_STATUS_OK;
}

void nad_elem_destroy_n(
    const nad_ElemOps *ops,
    size_t elem_size,
    void *base,
    size_t n,
    void *ctx
) {
    assert(elem_size > 0);
    assert(n == 0 || base);

    if (!ops || !ops->destroy) {
        return;
    }

    for (size_t i = 0; i < n; ++i) {
        ops->destroy(byte_offset_mut(base, elem_size, i), ctx);
    }
}

static const char *byte_offset(const void *base, size_t stride, size_t n) {
    return (const char *) base + stride * n;
}

static char *byte_offset_mut(void *base, size_t stride, size_t n) {
    return (char *) base + stride * n;
}
