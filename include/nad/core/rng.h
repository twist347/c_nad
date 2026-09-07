#pragma once

#include "nad/core/export.h"

#include <stddef.h>
#include <stdint.h>

/// @file

/// @defgroup core_rng core/rng
/// @ingroup core
/// @brief nad_Rng — a seeded generator, and the numbers drawn from one
///
/// xoshiro256++ over four words of state, seeded through SplitMix64. Fast and small, and
/// **not** cryptographic: a handful of outputs is enough to predict the rest.
///
/// A generator is a value and not a resource — transparent like nad_Span, so it takes no
/// nad_Al, returns no nad_Status and has nothing to drop. Copying one forks the sequence.
/// The same seed replays the same numbers, which is what makes a bug on the 4000th draw
/// reproducible; nothing here is thread-safe, and every draw mutates.
///
/// Every draw is uniform over the range it names, with no modulo bias — nad_rng_u32_max
/// says how.
///
/// @par Example
/// @snippet core/example_rng.c seed
/// @snippet core/example_rng.c draw
/// @snippet core/example_rng.c span
/// @{

/// A pseudorandom generator: four words of xoshiro256++ state, never all zero.
typedef struct {
    uint64_t s[4]; ///< the state; nad_rng_from_seed fills it and nothing else should
} nad_Rng;

/// @name seeding
/// @{

/// a generator started from 'seed'
/// @param seed any value, 0 included
/// @return the generator, by value
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Rng nad_rng_from_seed(uint64_t seed);

/// @}

/// @name raw draws
/// @{

/// a draw over the whole range of the type
/// @param self the generator, advanced by the draw
/// @return the value; costs a whole draw, of which this is the top half
/// @bigo{1}
[[nodiscard]] NAD_API
uint32_t nad_rng_u32(nad_Rng *self);

/// a draw over the whole range of the type
/// @param self the generator, advanced by the draw
/// @return the value
/// @bigo{1}
[[nodiscard]] NAD_API
uint64_t nad_rng_u64(nad_Rng *self);

/// @}

/// @name bounded ints
/// @{

/// uniform in [0, max]
/// @param self the generator
/// @param max the largest it may return; 0 returns 0
/// @return the value
/// @bigo{1} — Lemire's nearly divisionless method: one multiply, and a division only for
///          the one slot the range does not divide evenly. The modulo everyone reaches
///          for first costs the same and skews toward the low values.
[[nodiscard]] NAD_API
uint32_t nad_rng_u32_max(nad_Rng *self, uint32_t max);

/// uniform in [0, max]
/// @copydetails nad_rng_u32_max
[[nodiscard]] NAD_API
uint64_t nad_rng_u64_max(nad_Rng *self, uint64_t max);

/// uniform in [0, len), the shape an index comes in
/// @param self the generator
/// @param len how many there are; asserts len > 0
/// @return an index into a span of that length
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_rng_idx(nad_Rng *self, size_t len);

/// uniform in [lo, hi]
/// @param self the generator
/// @param lo the low end, included
/// @param hi the high end, included; asserts lo <= hi, and lo == hi returns lo
/// @return the value; the full width works, INT32_MIN to INT32_MAX included
/// @bigo{1}
[[nodiscard]] NAD_API
int32_t nad_rng_i32_range(nad_Rng *self, int32_t lo, int32_t hi);

/// uniform in [lo, hi]
/// @copydetails nad_rng_i32_range
[[nodiscard]] NAD_API
int64_t nad_rng_i64_range(nad_Rng *self, int64_t lo, int64_t hi);

/// @}

/// @name floats
/// @{

/// uniform in [0.0f, 1.0f)
/// @param self the generator
/// @return the value, one of the 2^24 multiples of 2^-24 the range holds
/// @bigo{1}
[[nodiscard]] NAD_API
float nad_rng_f32(nad_Rng *self);

/// uniform in [0.0, 1.0)
/// @param self the generator
/// @return the value, one of the 2^53 multiples of 2^-53 the range holds
/// @bigo{1}
[[nodiscard]] NAD_API
double nad_rng_f64(nad_Rng *self);

/// uniform in [lo, hi)
/// @param self the generator
/// @param lo the low end, included
/// @param hi the high end, excluded; asserts both finite, a finite width and lo <= hi,
///           and lo == hi returns lo
/// @return the value; scaling rounds, so a result landing on 'hi' is walked back below it
/// @bigo{1}
[[nodiscard]] NAD_API
float nad_rng_f32_range(nad_Rng *self, float lo, float hi);

/// uniform in [lo, hi)
/// @copydetails nad_rng_f32_range
[[nodiscard]] NAD_API
double nad_rng_f64_range(nad_Rng *self, double lo, double hi);

/// @}

/// @name bool
/// @{

/// true half the time
/// @param self the generator
/// @return the top bit of one draw
/// @bigo{1}
[[nodiscard]] NAD_API
bool nad_rng_bool(nad_Rng *self);

/// @}

/// @}
