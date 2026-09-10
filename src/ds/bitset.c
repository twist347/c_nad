#include "trs/ds/bitset.h"

#include "trs/core/util.h"

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

struct trs_BitSet {
    uint64_t *words;
    size_t nbits;
    size_t nwords;
    trs_Al *al;
};

[[nodiscard]]
static size_t words_for(size_t nbits);

/// how many bytes the words take, the size the block was asked for
[[nodiscard]]
static size_t words_bytes(const trs_BitSet *self);

/// hands the words back and leaves an empty universe on the same allocator
static void release_words(trs_BitSet *self);

[[nodiscard]]
static size_t word_of(size_t idx);

[[nodiscard]]
static uint64_t bit_of(size_t idx);

/// the live bits of the last word — all of them when nbits lands on a word boundary
[[nodiscard]]
static uint64_t tail_mask(size_t nbits);

static void clear_tail(trs_BitSet *self);

/// the word with the bits below 'from' masked off; 'from' must be inside the set
[[nodiscard]]
static uint64_t word_from(uint64_t word, size_t from);

/// where the lowest one bit of 'word' sits, given that 'word' is word number 'w'
[[nodiscard]]
static size_t idx_of_first_one(size_t w, uint64_t word);

/* ========== lifetime ========== */

trs_Status trs_bitset_new(size_t nbits, trs_Al *al, trs_BitSet **out) {
    assert(al);
    assert(out);

    trs_BitSet *obj = trs_alloc(al, sizeof(trs_BitSet));
    if (!obj) {
        return TRS_STATUS_ERR_NO_MEM;
    }

    const size_t nwords = words_for(nbits);
    uint64_t *words = nullptr;

    if (nwords > 0) {
        words = trs_calloc(al, nwords, sizeof(uint64_t));
        if (!words) {
            trs_dealloc(al, obj, sizeof(trs_BitSet));
            return TRS_STATUS_ERR_NO_MEM;
        }
    }

    obj->words = words;
    obj->nbits = nbits;
    obj->nwords = nwords;
    obj->al = al;

    ASSERT_BITSET(obj);

    *out = obj;

    return TRS_STATUS_OK;
}

void trs_bitset_drop(trs_BitSet *self) {
    if (!self) {
        return;
    }

    ASSERT_BITSET(self);

    trs_Al *al_copy = self->al;
    trs_dealloc(al_copy, self->words, words_bytes(self));
    trs_dealloc(al_copy, self, sizeof(trs_BitSet));
}

/* ========== copy ========== */

trs_Status trs_bitset_copy(const trs_BitSet *self, trs_BitSet **out) {
    ASSERT_BITSET(self);

    return trs_bitset_copy_with(self, self->al, out);
}

trs_Status trs_bitset_copy_with(const trs_BitSet *self, trs_Al *al, trs_BitSet **out) {
    ASSERT_BITSET(self);
    assert(al);
    assert(out);

    trs_BitSet *copy;
    const trs_Status st = trs_bitset_new(self->nbits, al, &copy);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    if (self->nwords > 0) {
        memcpy(copy->words, self->words, words_bytes(self));
    }

    *out = copy;

    return TRS_STATUS_OK;
}

trs_Status trs_bitset_copy_assign(const trs_BitSet *self, trs_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    if (self->nwords != other->nwords) {
        uint64_t *new_words = trs_realloc(
            other->al,
            other->words,
            words_bytes(other),
            words_bytes(self)
        );
        // a new_size of 0 hands the block back and answers null, which is not a failure
        if (self->nwords > 0 && !new_words) {
            return TRS_STATUS_ERR_NO_MEM;
        }

        other->words = new_words;
        other->nwords = self->nwords;
    }

    other->nbits = self->nbits;

    if (self->nwords > 0) {
        memcpy(other->words, self->words, words_bytes(self));
    }

    ASSERT_BITSET(other);

    return TRS_STATUS_OK;
}

trs_Status trs_bitset_move_assign(trs_BitSet *self, trs_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);

    if (self == other) {
        return TRS_STATUS_OK;
    }

    // one allocator: the words are handed over, universe and all. What 'other' held ends up in 'self' and is released
    // there, through the very allocator that made it
    if (self->al == other->al) {
        TRS_SWAP(*self, *other);
        release_words(self);

        ASSERT_BITSET(self);
        ASSERT_BITSET(other);

        return TRS_STATUS_OK;
    }

    // two allocators: the whole copy is built on the target's before anything of it is
    // touched, so a refusal leaves both as they were
    trs_BitSet *obj;
    const trs_Status st = trs_bitset_copy_with(self, other->al, &obj);
    if (TRS_STATUS_IS_ERR(st)) {
        return st;
    }

    TRS_SWAP(*other, *obj);
    trs_bitset_drop(obj);
    release_words(self);

    ASSERT_BITSET(self);
    ASSERT_BITSET(other);

    return TRS_STATUS_OK;
}

void trs_bitset_swap(trs_BitSet *self, trs_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->al == other->al);

    if (self == other) {
        return;
    }

    TRS_SWAP(*self, *other);

    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
}

/* ========== one bit ========== */

bool trs_bitset_test(const trs_BitSet *self, size_t idx) {
    ASSERT_BITSET(self);
    assert(idx < self->nbits);

    return (self->words[word_of(idx)] & bit_of(idx)) != 0;
}

void trs_bitset_set(trs_BitSet *self, size_t idx) {
    ASSERT_BITSET(self);
    assert(idx < self->nbits);

    self->words[word_of(idx)] |= bit_of(idx);
}

void trs_bitset_clear(trs_BitSet *self, size_t idx) {
    ASSERT_BITSET(self);
    assert(idx < self->nbits);

    self->words[word_of(idx)] &= ~bit_of(idx);
}

void trs_bitset_flip(trs_BitSet *self, size_t idx) {
    ASSERT_BITSET(self);
    assert(idx < self->nbits);

    self->words[word_of(idx)] ^= bit_of(idx);
}

void trs_bitset_set_to(trs_BitSet *self, size_t idx, bool val) {
    if (val) {
        trs_bitset_set(self, idx);
    } else {
        trs_bitset_clear(self, idx);
    }
}

/* ========== all bits ========== */

void trs_bitset_set_all(trs_BitSet *self) {
    ASSERT_BITSET(self);

    // a byte of ones is a word of ones, and memset over no bytes wants a real pointer
    if (self->nwords > 0) {
        memset(self->words, 0xFF, words_bytes(self));
    }

    clear_tail(self);
}

void trs_bitset_clear_all(trs_BitSet *self) {
    ASSERT_BITSET(self);

    if (self->nwords > 0) {
        memset(self->words, 0, words_bytes(self));
    }
}

void trs_bitset_flip_all(trs_BitSet *self) {
    ASSERT_BITSET(self);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] = ~self->words[w];
    }

    clear_tail(self);
}

/* ========== info ========== */

size_t trs_bitset_count(const trs_BitSet *self) {
    ASSERT_BITSET(self);

    size_t count = 0;
    for (size_t w = 0; w < self->nwords; ++w) {
        count += stdc_count_ones_ull(self->words[w]);
    }

    return count;
}

bool trs_bitset_any(const trs_BitSet *self) {
    ASSERT_BITSET(self);

    for (size_t w = 0; w < self->nwords; ++w) {
        if (self->words[w] != 0) {
            return true;
        }
    }

    return false;
}

bool trs_bitset_all(const trs_BitSet *self) {
    ASSERT_BITSET(self);

    for (size_t w = 0; w < self->nwords; ++w) {
        const uint64_t want = w + 1 == self->nwords ? tail_mask(self->nbits) : ~UINT64_C(0);
        if (self->words[w] != want) {
            return false;
        }
    }

    return true;
}

bool trs_bitset_none(const trs_BitSet *self) {
    ASSERT_BITSET(self);

    return !trs_bitset_any(self);
}

size_t trs_bitset_len(const trs_BitSet *self) {
    ASSERT_BITSET(self);

    return self->nbits;
}

trs_Al *trs_bitset_al(const trs_BitSet *self) {
    ASSERT_BITSET(self);

    return self->al;
}

/* ========== scan ========== */

bool trs_bitset_find_next(const trs_BitSet *self, size_t from, size_t *out_idx) {
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

bool trs_bitset_find_next_clear(const trs_BitSet *self, size_t from, size_t *out_idx) {
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

bool trs_bitset_eq(const trs_BitSet *a, const trs_BitSet *b) {
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

void trs_bitset_union(trs_BitSet *self, const trs_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] |= other->words[w];
    }
}

void trs_bitset_intersect(trs_BitSet *self, const trs_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] &= other->words[w];
    }
}

void trs_bitset_difference(trs_BitSet *self, const trs_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] &= ~other->words[w];
    }
}

void trs_bitset_symmetric_difference(trs_BitSet *self, const trs_BitSet *other) {
    ASSERT_BITSET(self);
    ASSERT_BITSET(other);
    assert(self->nbits == other->nbits);

    for (size_t w = 0; w < self->nwords; ++w) {
        self->words[w] ^= other->words[w];
    }
}

bool trs_bitset_is_subset(const trs_BitSet *self, const trs_BitSet *other) {
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

bool trs_bitset_intersects(const trs_BitSet *self, const trs_BitSet *other) {
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

void trs_bitset_fprint(const trs_BitSet *self, FILE *stream) {
    ASSERT_BITSET(self);
    assert(stream);

    fputc('{', stream);

    bool first = true;
    size_t idx;
    for (size_t from = 0; trs_bitset_find_next(self, from, &idx); from = idx + 1) {
        if (!first) {
            fputs(", ", stream);
        }
        first = false;
        fprintf(stream, "%zu", idx);
    }

    fputs("}\n", stream);
}

void trs_bitset_print(const trs_BitSet *self) {
    trs_bitset_fprint(self, stdout);
}

/* ========== internals ========== */

static size_t words_for(size_t nbits) {
    // not (nbits + 63) / 64: that addition overflows for an nbits near SIZE_MAX
    return nbits / WORD_BITS + (nbits % WORD_BITS != 0);
}

static void release_words(trs_BitSet *self) {
    trs_dealloc(self->al, self->words, words_bytes(self));
    self->words = nullptr;
    self->nbits = 0;
    self->nwords = 0;
}

static size_t words_bytes(const trs_BitSet *self) {
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

static void clear_tail(trs_BitSet *self) {
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
