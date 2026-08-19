#include "nad/core/cmp.h"

#include <assert.h>
#include <math.h>
#include <string.h>

/* ========== internals ========== */

#define DEFINE_CMP_INT(name, T)    \
    int nad_cmp_##name(T a, T b) { \
        return (a > b) - (a < b);  \
    }

#define DEFINE_CMP_FLOAT(name, T)     \
    int nad_cmp_##name(T a, T b) {    \
        const bool a_nan = isnan(a);  \
        const bool b_nan = isnan(b);  \
        if (a_nan || b_nan) {         \
            if (a_nan && b_nan) {     \
                return 0;             \
            }                         \
            return a_nan ? 1 : -1;    \
        }                             \
        return (a > b) - (a < b);     \
    }

#define DEFINE_EQ(name, T)                \
    bool nad_eq_##name(T a, T b) {        \
        return nad_cmp_##name(a, b) == 0; \
    }

#define DEFINE_CMP_FN(name, T)                                     \
    int nad_cmp_fn_##name(const void *lhs, const void *rhs) {      \
        assert(lhs);                                               \
        assert(rhs);                                               \
        return nad_cmp_##name(*(const T *) lhs, *(const T *) rhs); \
    }

// no T: swapping the operands of the ascending callback needs no type at all, which is
// also why cstr can use this one despite its pointer-to-pointer operands
#define DEFINE_CMP_FN_DESC(name)                                   \
    int nad_cmp_fn_desc_##name(const void *lhs, const void *rhs) { \
        return nad_cmp_fn_##name(rhs, lhs);                        \
    }

#define DEFINE_EQ_FN(name, T)                                     \
    bool nad_eq_fn_##name(const void *lhs, const void *rhs) {     \
        assert(lhs);                                              \
        assert(rhs);                                              \
        return nad_eq_##name(*(const T *) lhs, *(const T *) rhs); \
    }

#define DEFINE_INT_ALL(name, T) \
    DEFINE_CMP_INT(name, T)     \
    DEFINE_EQ(name, T)          \
    DEFINE_CMP_FN(name, T)      \
    DEFINE_CMP_FN_DESC(name)    \
    DEFINE_EQ_FN(name, T)

#define DEFINE_FLOAT_ALL(name, T) \
    DEFINE_CMP_FLOAT(name, T)     \
    DEFINE_EQ(name, T)            \
    DEFINE_CMP_FN(name, T)        \
    DEFINE_CMP_FN_DESC(name)      \
    DEFINE_EQ_FN(name, T)

/* ========== instantiation ========== */

DEFINE_INT_ALL(i8, int8_t)
DEFINE_INT_ALL(i16, int16_t)
DEFINE_INT_ALL(i32, int32_t)
DEFINE_INT_ALL(i64, int64_t)

DEFINE_INT_ALL(u8, uint8_t)
DEFINE_INT_ALL(u16, uint16_t)
DEFINE_INT_ALL(u32, uint32_t)
DEFINE_INT_ALL(u64, uint64_t)

DEFINE_INT_ALL(size, size_t)

DEFINE_FLOAT_ALL(f32, float)
DEFINE_FLOAT_ALL(f64, double)

/* ========== cstr ========== */

int nad_cmp_cstr(const char *a, const char *b) {
    if (a == b) {
        return 0; // both null, or the same string
    }
    if (!a) {
        return -1;
    }
    if (!b) {
        return 1;
    }

    const int c = strcmp(a, b);

    return (c > 0) - (c < 0); // strcmp only promises a sign, so narrow it to -1/0/+1
}

// the operand is a pointer to a pointer, so the cast needs a star that DEFINE_CMP_FN
// cannot spell — the remaining three forms come from the shared macros
int nad_cmp_fn_cstr(const void *lhs, const void *rhs) {
    assert(lhs);
    assert(rhs);

    return nad_cmp_cstr(*(const char *const *) lhs, *(const char *const *) rhs);
}

bool nad_eq_fn_cstr(const void *lhs, const void *rhs) {
    assert(lhs);
    assert(rhs);

    return nad_eq_cstr(*(const char *const *) lhs, *(const char *const *) rhs);
}

DEFINE_EQ(cstr, const char *)
DEFINE_CMP_FN_DESC(cstr)

#undef DEFINE_CMP_INT
#undef DEFINE_CMP_FLOAT
#undef DEFINE_EQ
#undef DEFINE_CMP_FN
#undef DEFINE_CMP_FN_DESC
#undef DEFINE_EQ_FN
#undef DEFINE_INT_ALL
#undef DEFINE_FLOAT_ALL
