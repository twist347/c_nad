#include "nad/ds/bitset.h"

#include <assert.h>
#include <stdbit.h>
#include <stdint.h>
#include <string.h>

/* ========== internals ========== */

static constexpr size_t WORD_BITS = 64;

// The bits above the last live one are not part of the set, and every read here counts on
// them being clear: count sums whole words, any tests whole words, find_next trusts the
// first one bit it sees. Only the ops that touch a whole word at once can dirty them —
// set_all and flip_all — and both end with clear_tail. The pairwise ops cannot: | & ^ and
// &~ of two clear tails is a clear tail.
#define ASSERT_BITSET(b)                                                                     \
    (assert(b),                                                                              \
    assert((b)->nwords == words_for((b)->nbits)),                                            \
    assert((b)->nwords > 0 || !(b)->words),                                                  \
    assert((b)->nwords == 0 || (b)->words),                                                  \
    assert((b)->nwords == 0 || ((b)->words[(b)->nwords - 1] & ~tail_mask((b)->nbits)) == 0), \
    assert((b)->al))

struct nad_BitSet {
    uint64_t *words;
    size_t nbits;
    size_t nwords;
    nad_Al *al;
};

[[nodiscard]]
static size_t words_for(size_t nbits);

/// how many bytes the words take, the size the block was asked for
[[nodiscard]]
static size_t words_bytes(const nad_BitSet *self);

[[nodiscard]]
static size_t word_of(size_t idx);

[[nodiscard]]
static uint64_t bit_of(size_t idx);

/// the live bits of the last word — all of them when nbits lands on a word boundary
[[nodiscard]]
static uint64_t tail_mask(size_t nbits);

static void clear_tail(nad_BitSet *self);

/// the word with the bits below 'from' masked off; 'from' must be inside the set
[[nodiscard]]
static uint64_t word_from(uint64_t word, size_t from);

/// where the lowest one bit of 'word' sits, given that 'word' is word number 'w'
[[nodiscard]]
static size_t idx_of_first_one(size_t w, uint64_t word);

/* ========== lifetime ========== */

nad_Status nad_bitset_new(size_t nbits, nad_Al *al, nad_BitSet **out) {
    assert(al);
    assert(out);

    nad_BitSet *obj = nad_alloc(al, sizeof(nad_BitSet));
    if (!obj) {
        return NAD_STATUS_ERR_NO_MEM;
    }

    const size_t nwords = words_for(nbits);
    uint64_t *words = nullptr;

    if (nwords > 0) {
        words = nad_calloc(al, nwords, sizeof(uint64_t));
        if (!words) {
            nad_dealloc(al, obj, sizeof(nad_BitSet));
            return NAD_STATUS_ERR_NO_MEM;
        }
    }

    obj->words = words;
    obj->nbits = nbits;
    obj->nwords = nwords;
    obj->al = al;

    ASSERT_BITSET(obj);

    *out = obj;

    return NAD_STATUS_OK;
}

void nad_bitset_drop(nad_BitSet *self) {
    if (!self) {
        return;
    }

    ASSERT_BITSET(self);

    nad_Al *al_copy = self->al;
    nad_dealloc(al_copy, self->words, words_bytes(self));
    nad_dealloc(al_copy, self, sizeof(nad_BitSet));
}

/* ========== copy ========== */

nad_Status nad_bitset_copy(const nad_BitSet *self, nad_BitSet **out) {
    ASSERT_BITSET(self);

    return nad_bitset_copy_with(self, self->al, out);
}

nad_Status nad_bitset_copy_with(const nad_BitSet *self, nad_Al *al, nad_BitSet **out) {
    ASSERT_BITSET(self);
    assert(al);
    assert(out);

    nad_BitSet *copy;
    const nad_Status st = nad_bitset_new(self->nbits, al, &copy);
    if (NAD_STATUS_IS_ERR(st)) {
        return st;
    }

    if (self->nwords > 0) {
        memcpy(copy->words, self->words, words_bytes(self));
    }

    *out = copy;

    return NAD_STATUS_OK;
}

nad_Status nad_bitset_copy_assign(const nad_BitSet *self, nad_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);

    if (self == other) {
        return NAD_STATUS_OK;
    }

    if (self->nwords != other->nwords) {
        uint64_t *new_words = nad_realloc(other->al, other->words, words_bytes(other), words_bytes(self));
        // a new_size of 0 hands the block back and answers null, which is not a failure
        if (self->nwords > 0 && !new_words) {
            return NAD_STATUS_ERR_NO_MEM;
        }

        other->words = new_words;
        other->nwords = self->nwords;
    }

    other->nbits = self->nbits;

    if (self->nwords > 0) {
        memcpy(other->words, self->words, words_bytes(self));
    }

    ASSERT_BITSET(other);

    return NAD_STATUS_OK;
}

/* ========== one bit ========== */

bool nad_bitset_test(const nad_BitSet *self, size_t idx) {
    ASSERT_BITSET(self);
    assert(idx < self->nbits);

    return (self->words[word_of(idx)] & bit_of(idx)) != 0;
}

void nad_bitset_set(nad_BitSet *self, size_t idx) {
    ASSERT_BITSET(self);
    assert(idx < self->nbits);

    self->words[word_of(idx)] |= bit_of(idx);
}

void nad_bitset_clear(nad_BitSet *self, size_t idx) {
    ASSERT_BITSET(self);
    assert(idx < self->nbits);

    self->words[word_of(idx)] &= ~bit_of(idx);
}

void nad_bitset_flip(nad_BitSet *self, size_t idx) {
    ASSERT_BITSET(self);
    assert(idx < self->nbits);

    self->words[word_of(idx)] ^= bit_of(idx);
}

void nad_bitset_assign(nad_BitSet *self, size_t idx, bool val) {
    if (val) {
        nad_bitset_set(self, idx);
    } else {
        nad_bitset_clear(self, idx);
    }
}

/* ========== all bits ========== */

void nad_bitset_set_all(nad_BitSet *self) {
    ASSERT_BITSET(self);

    // a byte of ones is a word of ones, and memset over no bytes wants a real pointer
    if (self->nwords > 0) {
        memset(self->words, 0xFF, words_bytes(self));
    }

    clear_tail(self);
}

void nad_bitset_clear_all(nad_BitSet *self) {
    ASSERT_BITSET(self);

    if (self->nwords > 0) {
        memset(self->words, 0, words_bytes(self));
    }
}

void nad_bitset_flip_all(nad_BitSet *self) {
    ASSERT_BITSET(self);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] = ~self->words[w];
    }

    clear_tail(self);
}

/* ========== info ========== */

size_t nad_bitset_count(const nad_BitSet *self) {
    ASSERT_BITSET(self);

    size_t count = 0;
    for (size_t w = 0; w < self->nwords; ++w) {
        count += stdc_count_ones_ull(self->words[w]);
    }

    return count;
}

bool nad_bitset_any(const nad_BitSet *self) {
    ASSERT_BITSET(self);

    for (size_t w = 0; w < self->nwords; ++w) {
        if (self->words[w] != 0) {
            return true;
        }
    }

    return false;
}

bool nad_bitset_all(const nad_BitSet *self) {
    ASSERT_BITSET(self);

    for (size_t w = 0; w < self->nwords; ++w) {
        const uint64_t want = w + 1 == self->nwords ? tail_mask(self->nbits) : ~UINT64_C(0);
        if (self->words[w] != want) {
            return false;
        }
    }

    return true;
}

bool nad_bitset_none(const nad_BitSet *self) {
    ASSERT_BITSET(self);

    return !nad_bitset_any(self);
}

size_t nad_bitset_len(const nad_BitSet *self) {
    ASSERT_BITSET(self);

    return self->nbits;
}

nad_Al *nad_bitset_al(const nad_BitSet *self) {
    ASSERT_BITSET(self);

    return self->al;
}

/* ========== scan ========== */

bool nad_bitset_find_next(const nad_BitSet *self, size_t from, size_t *out_idx) {
    ASSERT_BITSET(self);
    assert(out_idx);

    if (from >= self->nbits) {
        return false;
    }

    size_t w = word_of(from);
    uint64_t word = word_from(self->words[w], from);

    for (;;) {
        if (word != 0) {
            *out_idx = idx_of_first_one(w, word);
            return true;
        }

        if (++w == self->nwords) {
            return false;
        }

        word = self->words[w];
    }
}

bool nad_bitset_find_next_clear(const nad_BitSet *self, size_t from, size_t *out_idx) {
    ASSERT_BITSET(self);
    assert(out_idx);

    if (from >= self->nbits) {
        return false;
    }

    size_t w = word_of(from);
    uint64_t word = word_from(~self->words[w], from);

    for (;;) {
        if (word != 0) {
            const size_t idx = idx_of_first_one(w, word);

            // the complement turns the tail into ones, and those are not members
            if (idx >= self->nbits) {
                return false;
            }

            *out_idx = idx;
            return true;
        }

        if (++w == self->nwords) {
            return false;
        }

        word = ~self->words[w];
    }
}

/* ========== set ops ========== */

bool nad_bitset_eq(const nad_BitSet *a, const nad_BitSet *b) {
    ASSERT_BITSET(a);
    ASSERT_BITSET(b);

    if (a == b) {
        return true;
    }

    if (a->nbits != b->nbits) {
        return false;
    }

    return a->nwords == 0 || memcmp(a->words, b->words, words_bytes(a)) == 0;
}

void nad_bitset_union(nad_BitSet *self, const nad_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] |= other->words[w];
    }
}

void nad_bitset_intersect(nad_BitSet *self, const nad_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] &= other->words[w];
    }
}

void nad_bitset_difference(nad_BitSet *self, const nad_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] &= ~other->words[w];
    }
}

void nad_bitset_symmetric_difference(nad_BitSet *self, const nad_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] ^= other->words[w];
    }
}

bool nad_bitset_is_subset(const nad_BitSet *self, const nad_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        if ((self->words[w] & ~other->words[w]) != 0) {
            return false;
        }
    }

    return true;
}

bool nad_bitset_intersects(const nad_BitSet *self, const nad_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        if ((self->words[w] & other->words[w]) != 0) {
            return true;
        }
    }

    return false;
}

/* ========== print ========== */

void nad_bitset_fprint(const nad_BitSet *self, FILE *stream) {
    ASSERT_BITSET(self);
    assert(stream);

    fputc('{', stream);

    bool first = true;
    size_t idx;
    for (size_t from = 0; nad_bitset_find_next(self, from, &idx); from = idx + 1) {
        if (!first) {
            fputs(", ", stream);
        }
        first = false;
        fprintf(stream, "%zu", idx);
    }

    fputs("}\n", stream);
}

void nad_bitset_print(const nad_BitSet *self) {
    nad_bitset_fprint(self, stdout);
}

/* ========== internals ========== */

static size_t words_for(size_t nbits) {
    // not (nbits + 63) / 64: that addition overflows for an nbits near SIZE_MAX
    return nbits / WORD_BITS + (nbits % WORD_BITS != 0);
}

static size_t words_bytes(const nad_BitSet *self) {
    assert(self);

    return self->nwords * sizeof(uint64_t);
}

static size_t word_of(size_t idx) {
    return idx / WORD_BITS;
}

static uint64_t bit_of(size_t idx) {
    return UINT64_C(1) << (idx % WORD_BITS);
}

static uint64_t tail_mask(size_t nbits) {
    const size_t rem = nbits % WORD_BITS;

    // rem == 0 means the last word is full; writing it as a shift would be a shift by 64
    return rem == 0 ? ~UINT64_C(0) : (UINT64_C(1) << rem) - 1;
}

static void clear_tail(nad_BitSet *self) {
    assert(self);

    if (self->nwords > 0) {
        self->words[self->nwords - 1] &= tail_mask(self->nbits);
    }
}

static uint64_t word_from(uint64_t word, size_t from) {
    return word & (~UINT64_C(0) << (from % WORD_BITS));
}

static size_t idx_of_first_one(size_t w, uint64_t word) {
    // stdc_first_trailing_one_* counts from 1 and answers 0 for a word with no bits at
    // all, which is what this rules out
    assert(word != 0);

    return w * WORD_BITS + (stdc_first_trailing_one_ull(word) - 1);
}
