#pragma once

#include "nad/core/export.h"

#include <stdint.h>
#include <stddef.h>

/// @file

/// @defgroup core_hash core/hash
/// @ingroup core
/// @brief nad_Hasher, and ready-made hashers for the built-in types
///
/// A hasher takes one value by address and returns a nad_Hash. It travels paired with a
/// nad_Eq, under one law: what the equality calls equal must hash alike.
///
/// Three values would break that law if hashed as raw bytes, so they are canonicalized:
/// -0.0 hashes as +0.0, every NaN hashes as one quiet NaN, and a null string hashes as a
/// value of its own rather than as "" — the three distinctions core/cmp keeps.
///
/// nad_hash_bytes and nad_hash_combine are the two that are not nad_Hasher, and what a
/// hasher for your own struct is built out of.
///
/// @par Example
/// @snippet core/example_hash.c struct
/// @snippet core/example_hash.c use
/// @{

/// What a hasher returns. Wide on purpose: a container takes the low bits it needs.
typedef uint64_t nad_Hash;

/// Hashes the value at its operand. Named for the agent rather than the operation,
/// because nad_Hash is already the name of the result.
typedef nad_Hash (*nad_Hasher)(const void *);


/// @name int
/// @{

/// a int8_t
/// @param val the address of the value
/// @return its hash
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Hash nad_hash_i8(const void *val);

/// a int16_t
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_i16(const void *val);

/// a int32_t
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_i32(const void *val);

/// a int64_t
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_i64(const void *val);


/// @}

/// @name uint
/// @{

/// a uint8_t
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_u8(const void *val);

/// a uint16_t
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_u16(const void *val);

/// a uint32_t
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_u32(const void *val);

/// a uint64_t
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_u64(const void *val);


/// @}

/// @name size
/// @{

/// a size_t
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_size(const void *val);


/// @}

/// @name ptrdiff
/// @{

/// a ptrdiff_t
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_ptrdiff(const void *val);


/// @}

/// @name floating point
/// @{

/// a float, canonicalized so the hash agrees with nad_eq_f32
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_f32(const void *val);

/// a double, as nad_hash_f32
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_f64(const void *val);


/// @}

/// @name char
/// @{

/// a char
/// @copydetails nad_hash_i8
[[nodiscard]] NAD_API
nad_Hash nad_hash_char(const void *val);

/// @}

/// @name bytes
/// @{

/// 'len' bytes at 'data' — not a nad_Hasher, since it takes a length
/// @param data the bytes; may be null only when 'len' is 0
/// @param len how many bytes to read
/// @return their hash
/// @warning bytes agree with memcmp, not with nad_eq_<T>: padding is read, -0.0 and +0.0
///          hash apart, two equal NaNs hash apart. Build a struct's hash out of its
///          fields' instead
/// @bigo{n}
[[nodiscard]] NAD_API
nad_Hash nad_hash_bytes(const void *data, size_t len);

/// @}

/// @name str
/// @{

/// a string, by its contents; a null pointer hashes as a value of its own, not as ""
/// @param val the address of the value — a pointer to the const char *, not the string
/// @return its hash
/// @bigo{n} — n is the length of the string
[[nodiscard]] NAD_API
nad_Hash nad_hash_cstr(const void *val);

/// @}

/// @name combine
/// @{

/// folds two hashes into one, order-sensitively
/// @param a one hash
/// @param b the other
/// @return the folded hash
/// @warning both operands must already be hashes; fed raw values, it collides badly
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Hash nad_hash_combine(nad_Hash a, nad_Hash b);

/// @}

/// @}
