#pragma once

#include "nad/core/export.h"

/// comparator: <0 if a<b, 0 if a==b, >0 if a>b. qsort compatible
typedef int (*nad_Cmp)(const void *, const void *);

/// equality: true if a == b.
typedef bool (*nad_Eq)(const void *, const void *);

/*
 * Comparison and equality for the built-in types. Two forms per type:
 *
 *   ascending:  nad_cmp_<T> -> nad_Cmp,  nad_eq_<T> -> nad_Eq
 *   descending: nad_cmp_desc_<T> — the comparator with its operands swapped
 *
 * Operands always arrive as const void *, so these are what the algorithms take and
 * what qsort takes. There is no value-taking form: to compare two values, call the
 * comparator with their addresses — nad_cmp_i32(&a, &b) — which is also how a
 * comparator for a struct delegates to one for its field.
 *
 * f32 and f64 are float and double. That they are IEEE binary32/binary64 follows from
 * __STDC_IEC_559__, not from the standard itself.
 *
 * Float ordering is total, which the naive (a > b) - (a < b) is not: NaN sorts after
 * every non-NaN, NaN compares equal to NaN, and -0.0 compares equal to +0.0. Without
 * this a span holding a NaN has no strict weak ordering and sorting it is undefined.
 *
 * cstr operands are const char *; null is less than every non-null string, and two
 * nulls are equal. Being a pointer type, its operands are a pointer to the pointer.
 *
 * A type earns an entry only when no other entry is a portable substitute for it. Since
 * operands arrive erased, what matters is width and signedness, not type identity: a span
 * of long long is served by nad_cmp_i64, and one of bool by nad_cmp_u8. Three entries are
 * not fixed-width for that reason — char, whose signedness is implementation-defined, and
 * size and ptrdiff, whose width follows the target rather than the name.
 */

/* ========== comparators ========== */

/* ========== int ========== */

[[nodiscard]] NAD_API
int nad_cmp_i8(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_i16(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_i32(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_i64(const void *lhs, const void *rhs);

/* ========== uint ========== */

[[nodiscard]] NAD_API
int nad_cmp_u8(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_u16(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_u32(const void *lhs, const void *rhs);


[[nodiscard]] NAD_API
int nad_cmp_u64(const void *lhs, const void *rhs);

/* ========== size ========== */

[[nodiscard]] NAD_API
int nad_cmp_size(const void *lhs, const void *rhs);

/* ========== ptrdiff ========== */

[[nodiscard]] NAD_API
int nad_cmp_ptrdiff(const void *lhs, const void *rhs);

/* ========== floating point ========== */

[[nodiscard]] NAD_API
int nad_cmp_f32(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_f64(const void *lhs, const void *rhs);

/* ========== char ========== */

/// ordered by whatever signedness char has on the target
[[nodiscard]] NAD_API
int nad_cmp_char(const void *lhs, const void *rhs);

/* ========== str ========== */

/// operands point to const char *
[[nodiscard]] NAD_API
int nad_cmp_cstr(const void *lhs, const void *rhs);

/* ========== descending comparators ========== */

/* ========== int ========== */

[[nodiscard]] NAD_API
int nad_cmp_desc_i8(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_desc_i16(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_desc_i32(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_desc_i64(const void *lhs, const void *rhs);

/* ========== uint ========== */

[[nodiscard]] NAD_API
int nad_cmp_desc_u8(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_desc_u16(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_desc_u32(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_desc_u64(const void *lhs, const void *rhs);

/* ========== size ========== */

[[nodiscard]] NAD_API
int nad_cmp_desc_size(const void *lhs, const void *rhs);

/* ========== ptrdiff ========== */

[[nodiscard]] NAD_API
int nad_cmp_desc_ptrdiff(const void *lhs, const void *rhs);

/* ========== floating point ========== */

[[nodiscard]] NAD_API
int nad_cmp_desc_f32(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
int nad_cmp_desc_f64(const void *lhs, const void *rhs);

/* ========== char ========== */

[[nodiscard]] NAD_API
int nad_cmp_desc_char(const void *lhs, const void *rhs);

/* ========== str ========== */

[[nodiscard]] NAD_API
int nad_cmp_desc_cstr(const void *lhs, const void *rhs);

/* ========== equality ========== */

/* ========== int ========== */

[[nodiscard]] NAD_API
bool nad_eq_i8(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
bool nad_eq_i16(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
bool nad_eq_i32(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
bool nad_eq_i64(const void *lhs, const void *rhs);

/* ========== uint ========== */

[[nodiscard]] NAD_API
bool nad_eq_u8(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
bool nad_eq_u16(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
bool nad_eq_u32(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
bool nad_eq_u64(const void *lhs, const void *rhs);

/* ========== size ========== */

[[nodiscard]] NAD_API
bool nad_eq_size(const void *lhs, const void *rhs);

/* ========== ptrdiff ========== */

[[nodiscard]] NAD_API
bool nad_eq_ptrdiff(const void *lhs, const void *rhs);

/* ========== floating point ========== */

[[nodiscard]] NAD_API
bool nad_eq_f32(const void *lhs, const void *rhs);

[[nodiscard]] NAD_API
bool nad_eq_f64(const void *lhs, const void *rhs);

/* ========== char ========== */

[[nodiscard]] NAD_API
bool nad_eq_char(const void *lhs, const void *rhs);

/* ========== str ========== */

/// operands point to const char *
[[nodiscard]] NAD_API
bool nad_eq_cstr(const void *lhs, const void *rhs);
