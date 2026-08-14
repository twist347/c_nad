#pragma once

#include "nad/core/export.h"

#include <stddef.h>
#include <stdint.h>

/// comparator: <0 if a<b, 0 if a==b, >0 if a>b. qsort compatible
typedef int (*nad_CmpFn)(const void *, const void *);

typedef int (*nad_CmpCtxFn)(const void *, const void *, void *);

/// equality: true if a == b.
typedef bool (*nad_EqFn)(const void *, const void *);

/*
 * Comparison and equality for the built-in types. Three forms per type:
 *
 *   value:      nad_cmp_<T>(a, b) -> int (-1/0/+1),  nad_eq_<T>(a, b) -> bool
 *   callback:   nad_cmp_fn_<T> / nad_eq_fn_<T> — a nad_CmpFn / nad_EqFn, so also
 *               qsort compatible; operands arrive as const void *
 *   descending: nad_cmp_fn_desc_<T> — the callback with its operands swapped
 *
 * f32 and f64 are float and double. That they are IEEE binary32/binary64 follows from
 * __STDC_IEC_559__, not from the standard itself.
 *
 * Float ordering is total, which the naive (a > b) - (a < b) is not: NaN sorts after
 * every non-NaN, NaN compares equal to NaN, and -0.0 compares equal to +0.0. Without
 * this a span holding a NaN has no strict weak ordering and sorting it is undefined.
 *
 * cstr operands are const char *; null is less than every non-null string, and two
 * nulls are equal. The callback forms take a pointer to the pointer.
 */

/* ========== value comparators ========== */

[[nodiscard]] NAD_API int nad_cmp_i8(int8_t a, int8_t b);
[[nodiscard]] NAD_API int nad_cmp_i16(int16_t a, int16_t b);
[[nodiscard]] NAD_API int nad_cmp_i32(int32_t a, int32_t b);
[[nodiscard]] NAD_API int nad_cmp_i64(int64_t a, int64_t b);

[[nodiscard]] NAD_API int nad_cmp_u8(uint8_t a, uint8_t b);
[[nodiscard]] NAD_API int nad_cmp_u16(uint16_t a, uint16_t b);
[[nodiscard]] NAD_API int nad_cmp_u32(uint32_t a, uint32_t b);
[[nodiscard]] NAD_API int nad_cmp_u64(uint64_t a, uint64_t b);

[[nodiscard]] NAD_API int nad_cmp_size(size_t a, size_t b);

[[nodiscard]] NAD_API int nad_cmp_f32(float a, float b);
[[nodiscard]] NAD_API int nad_cmp_f64(double a, double b);

[[nodiscard]] NAD_API int nad_cmp_cstr(const char *a, const char *b);

/* ========== ascending callbacks ========== */

[[nodiscard]] NAD_API int nad_cmp_fn_i8(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_i16(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_i32(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_i64(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API int nad_cmp_fn_u8(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_u16(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_u32(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_u64(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API int nad_cmp_fn_size(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API int nad_cmp_fn_f32(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_f64(const void *lhs, const void *rhs);

/// operands point to const char *
[[nodiscard]] NAD_API int nad_cmp_fn_cstr(const void *lhs, const void *rhs);

/* ========== descending callbacks ========== */

[[nodiscard]] NAD_API int nad_cmp_fn_desc_i8(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_desc_i16(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_desc_i32(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_desc_i64(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API int nad_cmp_fn_desc_u8(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_desc_u16(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_desc_u32(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_desc_u64(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API int nad_cmp_fn_desc_size(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API int nad_cmp_fn_desc_f32(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API int nad_cmp_fn_desc_f64(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API int nad_cmp_fn_desc_cstr(const void *lhs, const void *rhs);

/* ========== value equality ========== */

[[nodiscard]] NAD_API bool nad_eq_i8(int8_t a, int8_t b);
[[nodiscard]] NAD_API bool nad_eq_i16(int16_t a, int16_t b);
[[nodiscard]] NAD_API bool nad_eq_i32(int32_t a, int32_t b);
[[nodiscard]] NAD_API bool nad_eq_i64(int64_t a, int64_t b);

[[nodiscard]] NAD_API bool nad_eq_u8(uint8_t a, uint8_t b);
[[nodiscard]] NAD_API bool nad_eq_u16(uint16_t a, uint16_t b);
[[nodiscard]] NAD_API bool nad_eq_u32(uint32_t a, uint32_t b);
[[nodiscard]] NAD_API bool nad_eq_u64(uint64_t a, uint64_t b);

[[nodiscard]] NAD_API bool nad_eq_size(size_t a, size_t b);

[[nodiscard]] NAD_API bool nad_eq_f32(float a, float b);
[[nodiscard]] NAD_API bool nad_eq_f64(double a, double b);

[[nodiscard]] NAD_API bool nad_eq_cstr(const char *a, const char *b);

/* ========== equality callbacks ========== */

[[nodiscard]] NAD_API bool nad_eq_fn_i8(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API bool nad_eq_fn_i16(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API bool nad_eq_fn_i32(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API bool nad_eq_fn_i64(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API bool nad_eq_fn_u8(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API bool nad_eq_fn_u16(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API bool nad_eq_fn_u32(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API bool nad_eq_fn_u64(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API bool nad_eq_fn_size(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API bool nad_eq_fn_f32(const void *lhs, const void *rhs);
[[nodiscard]] NAD_API bool nad_eq_fn_f64(const void *lhs, const void *rhs);

/// operands point to const char *
[[nodiscard]] NAD_API bool nad_eq_fn_cstr(const void *lhs, const void *rhs);

/* ========== macros ========== */

/*
 * size and cstr are absent on purpose. size_t is always the same type as whichever
 * fixed-width integer matches its width — uint64_t here, uint32_t on a 32-bit target —
 * and two compatible associations make _Generic a compile error, on every platform.
 * cstr is a pointer, and a string literal decays to char *, which would need its own
 * association. Call nad_cmp_size / nad_cmp_cstr by name.
 *
 * There is deliberately no default: an unmatched type should fail with the compiler
 * naming it. Watch for long long, which is not int64_t here even though both are 64 bits.
 */

#define NAD_CMP(a, b) _Generic((a), \
    int8_t:   nad_cmp_i8,           \
    int16_t:  nad_cmp_i16,          \
    int32_t:  nad_cmp_i32,          \
    int64_t:  nad_cmp_i64,          \
    uint8_t:  nad_cmp_u8,           \
    uint16_t: nad_cmp_u16,          \
    uint32_t: nad_cmp_u32,          \
    uint64_t: nad_cmp_u64,          \
    float:    nad_cmp_f32,          \
    double:   nad_cmp_f64           \
)((a), (b))

#define NAD_EQ(a, b) _Generic((a), \
    int8_t:   nad_eq_i8,           \
    int16_t:  nad_eq_i16,          \
    int32_t:  nad_eq_i32,          \
    int64_t:  nad_eq_i64,          \
    uint8_t:  nad_eq_u8,           \
    uint16_t: nad_eq_u16,          \
    uint32_t: nad_eq_u32,          \
    uint64_t: nad_eq_u64,          \
    float:    nad_eq_f32,          \
    double:   nad_eq_f64           \
)((a), (b))
