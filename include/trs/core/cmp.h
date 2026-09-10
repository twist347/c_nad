#pragma once

#include "trs/core/export.h"

/// @file

/// @defgroup core_cmp core/cmp
/// @ingroup core
/// @brief trs_Cmp and trs_Eq, and ready-made ones for the built-in types
///
/// Three forms per type: trs_cmp_<T> ascending, trs_cmp_desc_<T> the same with its
/// operands swapped, trs_eq_<T> only whether the two are equal.
///
/// Operands arrive as const void *, so these are what the algos take and what qsort
/// takes. There is deliberately no value-taking form: compare two values by calling with
/// their addresses — trs_cmp_i32(&a, &b) — which is also how a struct's comparator
/// delegates to a field's.
///
/// Float order is total, which the naive (a > b) - (a < b) is not: NaN after every
/// non-NaN, NaN equal to NaN, -0.0 equal to +0.0. Without that a span holding a NaN has
/// no strict weak ordering, and sorting it is undefined behaviour.
///
/// @par Example
/// @snippet core/example_cmp.c struct
/// @snippet core/example_cmp.c ready
/// @snippet core/example_cmp.c use
/// @{

/// A comparator: negative if lhs < rhs, zero if equal, positive if lhs > rhs. Shaped for
/// qsort, and what every ordering algo takes.
typedef int (*trs_Cmp)(const void *, const void *);

/// An equality: true when the two are equal. What the hash containers take.
typedef bool (*trs_Eq)(const void *, const void *);

/// @name comparators
/// @{

/* ========== int ========== */

/// two int8_t, ascending
/// @param lhs the left operand, by address
/// @param rhs the right operand, by address
/// @return negative, zero or positive as lhs orders before, with, or after rhs
/// @bigo{1}
[[nodiscard]] TRS_API
int trs_cmp_i8(const void *lhs, const void *rhs);

/// two int16_t, ascending
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_i16(const void *lhs, const void *rhs);

/// two int32_t, ascending
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_i32(const void *lhs, const void *rhs);

/// two int64_t, ascending
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_i64(const void *lhs, const void *rhs);

/* ========== uint ========== */

/// two uint8_t, ascending
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_u8(const void *lhs, const void *rhs);

/// two uint16_t, ascending
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_u16(const void *lhs, const void *rhs);

/// two uint32_t, ascending
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_u32(const void *lhs, const void *rhs);

/// two uint64_t, ascending
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_u64(const void *lhs, const void *rhs);

/* ========== size ========== */

/// two size_t, ascending
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_size(const void *lhs, const void *rhs);

/* ========== ptrdiff ========== */

/// two ptrdiff_t, ascending
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_ptrdiff(const void *lhs, const void *rhs);

/* ========== floating point ========== */

/// two float, ascending in the total order above
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_f32(const void *lhs, const void *rhs);

/// two double, as trs_cmp_f32
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_f64(const void *lhs, const void *rhs);

/* ========== char ========== */

/// two char, by whatever signedness char has on the target
/// @copydetails trs_cmp_i8
[[nodiscard]] TRS_API
int trs_cmp_char(const void *lhs, const void *rhs);

/* ========== str ========== */

/// two strings, in strcmp order; a null orders before every string, two nulls are equal
/// @param lhs the left operand — a pointer to the const char *, not the string
/// @param rhs the right operand, the same
/// @return negative, zero or positive as lhs orders before, with, or after rhs
/// @bigo{n} — n is the length of the shorter string
[[nodiscard]] TRS_API
int trs_cmp_cstr(const void *lhs, const void *rhs);

/// @}

/// @name descending comparators
/// @{

/* ========== int ========== */

/// trs_cmp_i8 with its operands swapped
/// @param lhs the left operand, by address
/// @param rhs the right operand, by address
/// @return negative, zero or positive as lhs orders before, with, or after rhs
/// @bigo{1}
[[nodiscard]] TRS_API
int trs_cmp_desc_i8(const void *lhs, const void *rhs);

/// trs_cmp_i16 with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_i16(const void *lhs, const void *rhs);

/// trs_cmp_i32 with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_i32(const void *lhs, const void *rhs);

/// trs_cmp_i64 with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_i64(const void *lhs, const void *rhs);

/* ========== uint ========== */

/// trs_cmp_u8 with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_u8(const void *lhs, const void *rhs);

/// trs_cmp_u16 with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_u16(const void *lhs, const void *rhs);

/// trs_cmp_u32 with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_u32(const void *lhs, const void *rhs);

/// trs_cmp_u64 with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_u64(const void *lhs, const void *rhs);

/* ========== size ========== */

/// trs_cmp_size with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_size(const void *lhs, const void *rhs);

/* ========== ptrdiff ========== */

/// trs_cmp_ptrdiff with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_ptrdiff(const void *lhs, const void *rhs);

/* ========== floating point ========== */

/// trs_cmp_f32 with its operands swapped, so NaN sorts first
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_f32(const void *lhs, const void *rhs);

/// trs_cmp_f64 with its operands swapped, so NaN sorts first
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_f64(const void *lhs, const void *rhs);

/* ========== char ========== */

/// trs_cmp_char with its operands swapped
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_char(const void *lhs, const void *rhs);

/* ========== str ========== */

/// trs_cmp_cstr with its operands swapped, so a null orders last
/// @copydetails trs_cmp_desc_i8
[[nodiscard]] TRS_API
int trs_cmp_desc_cstr(const void *lhs, const void *rhs);

/// @}

/// @name equality
/// @{

/* ========== int ========== */

/// two int8_t
/// @param lhs the left operand, by address
/// @param rhs the right operand, by address
/// @return whether the two are equal
/// @bigo{1}
[[nodiscard]] TRS_API
bool trs_eq_i8(const void *lhs, const void *rhs);

/// two int16_t
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_i16(const void *lhs, const void *rhs);

/// two int32_t
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_i32(const void *lhs, const void *rhs);

/// two int64_t
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_i64(const void *lhs, const void *rhs);

/* ========== uint ========== */

/// two uint8_t
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_u8(const void *lhs, const void *rhs);

/// two uint16_t
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_u16(const void *lhs, const void *rhs);

/// two uint32_t
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_u32(const void *lhs, const void *rhs);

/// two uint64_t
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_u64(const void *lhs, const void *rhs);

/* ========== size ========== */

/// two size_t
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_size(const void *lhs, const void *rhs);

/* ========== ptrdiff ========== */

/// two ptrdiff_t
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_ptrdiff(const void *lhs, const void *rhs);

/* ========== floating point ========== */

/// two float, equal when trs_cmp_f32 calls them equal
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_f32(const void *lhs, const void *rhs);

/// two double, as trs_eq_f32
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_f64(const void *lhs, const void *rhs);

/* ========== char ========== */

/// two char
/// @copydetails trs_eq_i8
[[nodiscard]] TRS_API
bool trs_eq_char(const void *lhs, const void *rhs);

/* ========== str ========== */

/// two strings, equal by contents; two nulls are equal, a null equals nothing else
/// @param lhs the left operand — a pointer to the const char *, not the string
/// @param rhs the right operand, the same
/// @return whether the two are equal
/// @bigo{n} — n is the length of the shorter string
[[nodiscard]] TRS_API
bool trs_eq_cstr(const void *lhs, const void *rhs);

/// @}

/// @}
