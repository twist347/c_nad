#pragma once

#include "nad/core/util.h"

#include <stdint.h>

/* ========== type ========== */

/// an elem wider than a word, to keep elem_size honest. Deliberately spelled without the
/// nad_test_ prefix of the rest of this layer: it is written as a type argument
/// (NAD_SPAN_NEW_MUT(Pair, buf, 2)) in every test that has one, where a prefix is noise.
typedef struct {
    int64_t a;
    int64_t b;
} Pair;

/* ========== predicates ========== */

/// nad_Pred over Pair, reading the first field
static inline bool nad_test_pair_a_is_positive(const void *elem, void *ctx) {
    NAD_UNUSED(ctx);

    return ((const Pair *) elem)->a > 0;
}

/// the negation is its own predicate rather than an inversion of the one above —
/// the same reason nad_span_find_if_not does not exist
static inline bool nad_test_pair_a_is_negative(const void *elem, void *ctx) {
    NAD_UNUSED(ctx);

    return ((const Pair *) elem)->a < 0;
}

/* ========== equalities ========== */

/// nad_Eq over Pair, reading the first field alone. Two Pairs equal under it can still
/// differ byte for byte, which is exactly what parts an _eq_by from an _eq
static inline bool nad_test_pair_eq_a(const void *lhs, const void *rhs) {
    return ((const Pair *) lhs)->a == ((const Pair *) rhs)->a;
}
