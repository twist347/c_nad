#pragma once

#include "nad/core/export.h"

#include <stdint.h>
#include <stddef.h>

typedef uint64_t nad_Hash;

/// hashes the value at its operand. Named for the agent, not the operation, because
/// nad_Hash is already the result: values equal under the matching nad_Eq must hash alike
typedef nad_Hash (*nad_Hasher)(const void *);

/* ========== int ========== */

[[nodiscard]] NAD_API
nad_Hash nad_hash_i8(const void *x);

[[nodiscard]] NAD_API
nad_Hash nad_hash_i16(const void *x);

[[nodiscard]] NAD_API
nad_Hash nad_hash_i32(const void *x);

[[nodiscard]] NAD_API
nad_Hash nad_hash_i64(const void *x);

/* ========== uint ========== */

[[nodiscard]] NAD_API
nad_Hash nad_hash_u8(const void *x);

[[nodiscard]] NAD_API
nad_Hash nad_hash_u16(const void *x);

[[nodiscard]] NAD_API
nad_Hash nad_hash_u32(const void *x);

[[nodiscard]] NAD_API
nad_Hash nad_hash_u64(const void *x);

/* ========== size ========== */

[[nodiscard]] NAD_API
nad_Hash nad_hash_usize(const void *x);

/* ========== ptrdiff ========== */

[[nodiscard]] NAD_API
nad_Hash nad_hash_isize(const void *x);

/* ========== floating point ========== */

[[nodiscard]] NAD_API
nad_Hash nad_hash_f32(const void *x);

[[nodiscard]] NAD_API
nad_Hash nad_hash_f64(const void *x);

/* ========== char ========== */

[[nodiscard]] NAD_API
nad_Hash nad_hash_char(const void *x);

/* ========== bytes ========== */

/// hashes 'len' bytes at 'data'; 'data' may be null only for an empty range. Not a
/// nad_Hasher — it takes a length — and the primitive the string form is built on.
///
/// Hashing an object's bytes agrees with memcmp, not with nad_eq_<T>: a struct's padding
/// is read, -0.0 and +0.0 hash apart, and two equal NaNs hash apart. Reach for it when
/// the bytes really are the value, and build the hash out of the fields' otherwise.
[[nodiscard]] NAD_API
nad_Hash nad_hash_bytes(const void *data, size_t len);

/* ========== str ========== */

[[nodiscard]] NAD_API
nad_Hash nad_hash_cstr(const void *x);

/* ========== combine ========== */

/// folds two hashes into one, order-sensitively — the way a struct's hash is built from
/// its fields'. Both operands must already be hashes: fed raw values, it collides badly.
[[nodiscard]] NAD_API
nad_Hash nad_hash_combine(nad_Hash x, nad_Hash y);
