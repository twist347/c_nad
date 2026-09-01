#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"
#include "nad/core/status.h"

#include <stddef.h>
#include <stdio.h>

/// @file

/// @defgroup ds_bitset ds/bitset
/// @ingroup ds
/// @brief nad_BitSet — a fixed set of indices, one bit each
///
/// The universe is named once: a bitset over 'nbits' holds a subset of 0 .. nbits - 1 and
/// never grows, the line ds/arr draws against ds/vec.
///
/// Membership is a bit, so there is no elem size, no comparator and no hasher: an index
/// is its own key. That is what this buys over ds/hset — a word holds 64 members, and the
/// union of two sets is a loop over words rather than over members.
///
/// Only the constructors allocate, so nothing else can fail, and an index out of range
/// asserts. The bits above nbits are kept clear, so count, all and the scans never see
/// them.
///
/// @par Example
/// @snippet ds/example_bitset.c build
/// @snippet ds/example_bitset.c bits
/// @snippet ds/example_bitset.c scan
/// @snippet ds/example_bitset.c set ops
/// @{

/// A fixed set of indices, one bit each.
/// An opaque handle: it comes from a constructor and goes back to nad_bitset_drop
typedef struct nad_BitSet nad_BitSet;

/// @name lifetime
/// @{

/// a new bitset over 0 .. nbits - 1 with nothing in it
/// @param nbits the size of the universe; 0 owns no words and holds nothing
/// @param al the allocator, kept for everything after
/// @param[out] out the new bitset, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the words cannot be allocated
/// @bigo{n/64}
[[nodiscard]] NAD_API
nad_Status nad_bitset_new(size_t nbits, nad_Al *al, nad_BitSet **out);

/// releases the words and the bitset through the allocator it was built with
/// @param self null is a no-op, so this is safe on a partly built object
/// @bigo{1}
NAD_API
void nad_bitset_drop(nad_BitSet *self);

/// @}

/// @name copy
/// @{

/// a new bitset with the same universe and members, on the same allocator
/// @param self the bitset to copy
/// @param[out] out the new bitset, written only on success
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the header or the words cannot be allocated
/// @bigo{n/64}
[[nodiscard]] NAD_API
nad_Status nad_bitset_copy(const nad_BitSet *self, nad_BitSet **out);

/// overwrites 'other' with 'self', resizing its words when the universes differ
/// @param self the bitset to copy from
/// @param[in,out] other keeps its own allocator; 'self' == 'other' is a no-op
/// @retval NAD_STATUS_OK on success
/// @retval NAD_STATUS_ERR_NO_MEM when the words cannot be resized, leaving 'other' as
///         it was
/// @bigo{n/64}
[[nodiscard]] NAD_API
nad_Status nad_bitset_copy_assign(const nad_BitSet *self, nad_BitSet *other);

/// @}

/// @name one bit
/// @{

/// whether 'idx' is in the set
/// @param self the bitset
/// @param idx asserts idx < len — out of range is a programmer error, not a status
/// @return whether the bit is set
/// @bigo{1}
[[nodiscard]] NAD_API
bool nad_bitset_test(const nad_BitSet *self, size_t idx);

/// puts 'idx' in the set
/// @param self the bitset
/// @param idx asserts idx < len
/// @bigo{1}
NAD_API
void nad_bitset_set(nad_BitSet *self, size_t idx);

/// takes 'idx' out of the set
/// @copydetails nad_bitset_set
NAD_API
void nad_bitset_clear(nad_BitSet *self, size_t idx);

/// puts 'idx' in when it is out and out when it is in
/// @copydetails nad_bitset_set
NAD_API
void nad_bitset_flip(nad_BitSet *self, size_t idx);

/// sets the bit at 'idx' to 'val'
/// @param self the bitset
/// @param idx asserts idx < len
/// @param val unlike nad_bitset_flip, asking twice changes nothing
/// @bigo{1}
NAD_API
void nad_bitset_assign(nad_BitSet *self, size_t idx, bool val);

/// @}

/// @name all bits
/// @{

/// puts every index of the universe in the set
/// @param self the bitset
/// @bigo{n/64}
NAD_API
void nad_bitset_set_all(nad_BitSet *self);

/// empties the set
/// @param self the bitset
/// @bigo{n/64}
NAD_API
void nad_bitset_clear_all(nad_BitSet *self);

/// replaces the set with its complement, taken over the universe and nothing above it
/// @param self the bitset
/// @bigo{n/64}
NAD_API
void nad_bitset_flip_all(nad_BitSet *self);

/// @}

/// @name info
/// @{

/// how many indices are in the set
/// @param self the bitset
/// @return the count, 0 when empty
/// @bigo{n/64}
[[nodiscard]] NAD_API
size_t nad_bitset_count(const nad_BitSet *self);

/// whether the set holds anything
/// @param self the bitset
/// @return false for an empty universe
/// @bigo{n/64}
[[nodiscard]] NAD_API
bool nad_bitset_any(const nad_BitSet *self);

/// whether the set is the whole universe
/// @param self the bitset
/// @return true for an empty universe, which holds all of nothing
/// @bigo{n/64}
[[nodiscard]] NAD_API
bool nad_bitset_all(const nad_BitSet *self);

/// whether the set is empty
/// @param self the bitset
/// @return the negation of nad_bitset_any
/// @bigo{n/64}
[[nodiscard]] NAD_API
bool nad_bitset_none(const nad_BitSet *self);

/// the size of the universe, as named at construction
/// @param self the bitset
/// @return nbits — moved only by nad_bitset_copy_assign
/// @bigo{1}
[[nodiscard]] NAD_API
size_t nad_bitset_len(const nad_BitSet *self);

/// the allocator the bitset was built with
/// @param self the bitset
/// @return the allocator, borrowed
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_bitset_al(const nad_BitSet *self);

/// @}

/// @name scan
/// @{

/// the first index in the set at or after 'from'
/// @param self the bitset
/// @param from a 'from' past the universe is a miss, not an assert, so a walk needs no
///             bound of its own
/// @param[out] out_idx written only on a hit
/// @return whether there is one
/// @bigo{n/64}
[[nodiscard]] NAD_API
bool nad_bitset_find_next(const nad_BitSet *self, size_t from, size_t *out_idx);

/// the first index outside the set at or after 'from'
/// @param self the bitset
/// @param from a 'from' past the universe is a miss
/// @param[out] out_idx written only on a hit
/// @return false for a full set: the bits above the universe are not indices of it
/// @bigo{n/64}
[[nodiscard]] NAD_API
bool nad_bitset_find_next_clear(const nad_BitSet *self, size_t from, size_t *out_idx);

/// @}

/// @name set ops
/// @{

/// whether the two name the same universe and hold the same members
/// @param a one bitset
/// @param b a different len is simply not equal, not an assert
/// @return whether they are equal
/// @bigo{n/64}
[[nodiscard]] NAD_API
bool nad_bitset_eq(const nad_BitSet *a, const nad_BitSet *b);

/// adds every member of 'other' to 'self'
/// @param[in,out] self the bitset written into
/// @param other asserts the same len — one universe, or these ops mean nothing.
///              'self' == 'other' is allowed and changes nothing
/// @bigo{n/64}
NAD_API
void nad_bitset_union(nad_BitSet *self, const nad_BitSet *other);

/// drops from 'self' everything that is not in 'other'
/// @param[in,out] self the bitset written into
/// @param other asserts the same len
/// @bigo{n/64}
NAD_API
void nad_bitset_intersect(nad_BitSet *self, const nad_BitSet *other);

/// drops from 'self' everything that is in 'other'
/// @param[in,out] self the bitset written into
/// @param other asserts the same len; 'self' == 'other' empties the set
/// @bigo{n/64}
NAD_API
void nad_bitset_difference(nad_BitSet *self, const nad_BitSet *other);

/// keeps in 'self' what is in exactly one of the two
/// @copydetails nad_bitset_difference
NAD_API
void nad_bitset_symmetric_difference(nad_BitSet *self, const nad_BitSet *other);

/// whether every member of 'self' is a member of 'other'
/// @param self the bitset that may be contained
/// @param other asserts the same len
/// @return true when 'self' is empty
/// @bigo{n/64}
[[nodiscard]] NAD_API
bool nad_bitset_is_subset(const nad_BitSet *self, const nad_BitSet *other);

/// whether the two share a member
/// @param self one bitset
/// @param other asserts the same len
/// @return whether the intersection is non-empty
/// @bigo{n/64}
[[nodiscard]] NAD_API
bool nad_bitset_intersects(const nad_BitSet *self, const nad_BitSet *other);

/// @}

/// @name print
/// @{

/// writes the members as {0, 3, 7} and a newline — the indices, not a row of bits
/// @param self the bitset
/// @param stream where to write
/// @bigo{n}
NAD_API
void nad_bitset_fprint(const nad_BitSet *self, FILE *stream);

/// nad_bitset_fprint to stdout
/// @param self the bitset
/// @bigo{n}
NAD_API
void nad_bitset_print(const nad_BitSet *self);

/// @}

/// @}
