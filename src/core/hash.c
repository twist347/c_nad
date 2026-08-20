#include "nad/core/hash.h"

#include <assert.h>
#include <math.h>
#include <string.h>

/* ========== internals ========== */

// FNV-1a 64-bit
static constexpr uint64_t NAD_FNV_OFFSET_BASIS_64 = UINT64_C(14695981039346656037);
static constexpr uint64_t NAD_FNV_PRIME_64 = UINT64_C(1099511628211);

// MurmurHash3 fmix64 constants
static constexpr uint64_t NAD_FMIX64_C1 = UINT64_C(0xff51afd7ed558ccd);
static constexpr uint64_t NAD_FMIX64_C2 = UINT64_C(0xc4ceb9fe1a85ec53);

static constexpr uint64_t NAD_HASH_GOLDEN_64 = UINT64_C(0x9e3779b97f4a7c15);

// the mixer's seed is deliberately not the constant nad_hash_combine folds in: with both
// being the golden ratio the two cancelled, and combine(0, 0) landed back on 0
static constexpr uint64_t NAD_HASH_SEED_64 = UINT64_C(0xa0761d6478bd642f);

// quiet NaN bit patterns
static constexpr uint32_t NAD_NAN_CANON_32 = UINT32_C(0x7fc00000);
static constexpr uint64_t NAD_NAN_CANON_64 = UINT64_C(0x7ff8000000000000);

[[nodiscard]]
static nad_Hash hash_mix_u64(uint64_t x);

// every integer type is the same operation: read it, widen it, mix it. Widening a signed
// type sign-extends, which is a bijection into uint64_t, so equal values stay equal and
// different ones stay different — all the mixer needs.
#define DEFINE_HASH_INT(name, T)                        \
    nad_Hash nad_hash_##name(const void *x) {           \
        assert(x);                                      \
        return hash_mix_u64((uint64_t) *(const T *) x); \
    }

/* ========== int ========== */

DEFINE_HASH_INT(i8, int8_t)
DEFINE_HASH_INT(i16, int16_t)
DEFINE_HASH_INT(i32, int32_t)
DEFINE_HASH_INT(i64, int64_t)

/* ========== uint ========== */

DEFINE_HASH_INT(u8, uint8_t)
DEFINE_HASH_INT(u16, uint16_t)
DEFINE_HASH_INT(u32, uint32_t)
DEFINE_HASH_INT(u64, uint64_t)

/* ========== size ========== */

DEFINE_HASH_INT(size, size_t)

/* ========== ptrdiff ========== */

DEFINE_HASH_INT(ptrdiff, ptrdiff_t)

/* ========== floating point ========== */

nad_Hash nad_hash_f32(const void *x) {
    assert(x);

    float val = *(const float *) x;
    if (val == 0.f) {
        val = 0.f;
    }

    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));

    // every NaN is equal to every other one here, so they must all hash alike — but not
    // alike to zero, which folding them onto 0.f would have done
    if (isnan(val)) {
        bits = NAD_NAN_CANON_32;
    }

    return hash_mix_u64(bits);
}

nad_Hash nad_hash_f64(const void *x) {
    assert(x);

    double val = *(const double *) x;
    if (val == 0.) {
        val = 0.;
    }

    uint64_t bits;
    memcpy(&bits, &val, sizeof(bits));

    if (isnan(val)) {
        bits = NAD_NAN_CANON_64;
    }

    return hash_mix_u64(bits);
}

/* ========== char ========== */

// read as unsigned char, not as char: the byte is the same either way, but char's
// signedness is the target's business and would otherwise leak into the hash — the same
// letter would hash differently on x86 and on ARM. Character types may alias freely, so
// reading a char object through unsigned char is fine.
DEFINE_HASH_INT(char, unsigned char)

/* ========== bytes ========== */

// FNV-1a, then the shared mixer: the one form here that cannot be a single mix, since the
// operand is a sequence. Bytes are read as unsigned char, so char's signedness stays out
// of the result.
nad_Hash nad_hash_bytes(const void *data, size_t len) {
    assert(data || len == 0);

    const unsigned char *bytes = data;
    uint64_t h = NAD_FNV_OFFSET_BASIS_64;

    for (size_t i = 0; i < len; ++i) {
        h ^= bytes[i];
        h *= NAD_FNV_PRIME_64;
    }

    return hash_mix_u64(h);
}

/* ========== str ========== */

nad_Hash nad_hash_cstr(const void *x) {
    assert(x);

    const char *str = *(const char *const *) x;

    // null is a value of its own, not the empty string — nad_eq_cstr keeps them apart,
    // so their hashes must be free to differ too
    if (!str) {
        return hash_mix_u64(0);
    }

    return nad_hash_bytes(str, strlen(str));
}

/* ========== combine ========== */

nad_Hash nad_hash_combine(nad_Hash x, nad_Hash y) {
    nad_Hash v = x;
    v ^= y + NAD_HASH_GOLDEN_64 + (v << 6) + (v >> 2);
    return hash_mix_u64(v);
}

/* ========== internals ========== */

static nad_Hash hash_mix_u64(uint64_t x) {
    x ^= NAD_HASH_SEED_64; // fmix64 maps 0 to 0, and 0 is the most common key there is
    x ^= x >> 33;
    x *= NAD_FMIX64_C1;
    x ^= x >> 33;
    x *= NAD_FMIX64_C2;
    x ^= x >> 33;
    return x;
}

#undef DEFINE_HASH_INT
