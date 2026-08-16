#include "nad/algo/sort.h"
#include "nad/alloc/alloc_arena.h"
#include "nad/alloc/alloc_default.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp() {
}

void tearDown() {
}

// ordered by key only, so tag can witness whether equal elements kept their order
typedef struct {
    int32_t key;
    int32_t tag;
} Tagged;

static int cmp_tagged(const void *a, const void *b) {
    return nad_cmp_i32(((const Tagged *) a)->key, ((const Tagged *) b)->key);
}

// sorts a copy with the insertion sort covered above — an oracle for the other sorts
static void sorted_copy(int32_t *dst, const int32_t *src, size_t n) {
    memcpy(dst, src, n * sizeof(int32_t));
    nad_span_insertion_sort(NAD_SPAN_NEW_MUT(int32_t, dst, n), nad_cmp_fn_i32);
}

// true when both hold the same elems, order aside — a reordering algorithm must never
// invent, drop or duplicate one
static bool same_elems(const int32_t *a, const int32_t *b, size_t n) {
    int32_t x[64];
    int32_t y[64];
    TEST_ASSERT_TRUE(n <= 64);

    sorted_copy(x, a, n);
    sorted_copy(y, b, n);

    return memcmp(x, y, n * sizeof(int32_t)) == 0;
}

/* ========== insertion_sort ========== */

static void test_insertion_sort_orders_a_shuffled_span() {
    int32_t buf[6] = {5, 3, 1, 4, 2, 6};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    nad_span_insertion_sort(s, nad_cmp_fn_i32);

    constexpr int32_t expected[6] = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 6);
}

static void test_insertion_sort_already_sorted_is_unchanged() {
    int32_t buf[4] = {1, 2, 3, 4};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 4);

    nad_span_insertion_sort(s, nad_cmp_fn_i32);

    constexpr int32_t expected[4] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 4);
}

// the worst case for insertion sort — every element travels the whole way
static void test_insertion_sort_reversed_span() {
    int32_t buf[5] = {5, 4, 3, 2, 1};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_insertion_sort(s, nad_cmp_fn_i32);

    constexpr int32_t expected[5] = {1, 2, 3, 4, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 5);
}

static void test_insertion_sort_keeps_duplicates() {
    int32_t buf[6] = {3, 1, 3, 2, 1, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    nad_span_insertion_sort(s, nad_cmp_fn_i32);

    constexpr int32_t expected[6] = {1, 1, 2, 3, 3, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 6);
}

static void test_insertion_sort_empty_and_single_are_noop() {
    int32_t buf[1] = {42};

    nad_span_insertion_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 0), nad_cmp_fn_i32);
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);

    nad_span_insertion_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 1), nad_cmp_fn_i32);
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
}

// the comparator defines the order — the algorithm must not assume ascending
static void test_insertion_sort_follows_the_comparator() {
    int32_t buf[5] = {2, 5, 1, 4, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_insertion_sort(s, nad_cmp_fn_desc_i32);

    constexpr int32_t expected[5] = {5, 4, 3, 2, 1};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 5);
}

// stability: equal keys must come out in their original relative order
static void test_insertion_sort_is_stable() {
    Tagged buf[5] = {
        {2, 0}, {1, 0}, {2, 1}, {1, 1}, {2, 2},
    };
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(Tagged, buf, 5);

    nad_span_insertion_sort(s, cmp_tagged);

    TEST_ASSERT_EQUAL_INT32(1, buf[0].key);
    TEST_ASSERT_EQUAL_INT32(0, buf[0].tag);
    TEST_ASSERT_EQUAL_INT32(1, buf[1].key);
    TEST_ASSERT_EQUAL_INT32(1, buf[1].tag);
    TEST_ASSERT_EQUAL_INT32(2, buf[2].key);
    TEST_ASSERT_EQUAL_INT32(0, buf[2].tag);
    TEST_ASSERT_EQUAL_INT32(2, buf[3].key);
    TEST_ASSERT_EQUAL_INT32(1, buf[3].tag);
    TEST_ASSERT_EQUAL_INT32(2, buf[4].key);
    TEST_ASSERT_EQUAL_INT32(2, buf[4].tag);
}

static void test_insertion_sort_stays_within_the_subspan() {
    int32_t buf[5] = {9, 3, 1, 2, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_insertion_sort(nad_span_sub_mut(s, 1, 3), nad_cmp_fn_i32);

    constexpr int32_t expected[5] = {9, 1, 2, 3, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 5);
}

/* ========== sort ========== */

static void test_sort_orders_a_shuffled_span() {
    int32_t buf[6] = {5, 3, 1, 4, 2, 6};

    nad_span_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 6), nad_cmp_fn_i32);

    constexpr int32_t want[6] = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 6);
}

static void test_sort_keeps_duplicates() {
    int32_t buf[6] = {3, 1, 3, 2, 1, 3};

    nad_span_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 6), nad_cmp_fn_i32);

    constexpr int32_t want[6] = {1, 1, 2, 3, 3, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 6);
}

static void test_sort_empty_and_single_are_noop() {
    int32_t buf[1] = {42};

    nad_span_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 0), nad_cmp_fn_i32);
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);

    nad_span_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 1), nad_cmp_fn_i32);
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
}

static void test_sort_follows_the_comparator() {
    int32_t buf[5] = {2, 5, 1, 4, 3};

    nad_span_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 5), nad_cmp_fn_desc_i32);

    constexpr int32_t want[5] = {5, 4, 3, 2, 1};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

static void test_sort_stays_within_the_subspan() {
    int32_t buf[5] = {9, 3, 1, 2, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_sort(nad_span_sub_mut(s, 1, 3), nad_cmp_fn_i32);

    constexpr int32_t want[5] = {9, 1, 2, 3, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

// long enough to exercise the real partitioning rather than a base case
static void test_sort_orders_a_long_span() {
    int32_t buf[64];
    for (size_t i = 0; i < 64; ++i) {
        // 37 is coprime with 64, so this is a scrambled permutation of 0..63
        buf[i] = (int32_t) ((i * 37 + 11) % 64);
    }

    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 64);
    TEST_ASSERT_FALSE(nad_span_is_sorted(nad_span_from_mut(s), nad_cmp_fn_i32));

    nad_span_sort(s, nad_cmp_fn_i32);

    TEST_ASSERT_TRUE(nad_span_is_sorted(nad_span_from_mut(s), nad_cmp_fn_i32));
    for (size_t i = 0; i < 64; ++i) {
        TEST_ASSERT_EQUAL_INT32((int32_t) i, buf[i]);
    }
}

// elem_size drives the moves, so a type wider than a word must travel whole
static void test_sort_moves_whole_elems() {
    Tagged buf[3] = {{3, 30}, {1, 10}, {2, 20}};

    nad_span_sort(NAD_SPAN_NEW_MUT(Tagged, buf, 3), cmp_tagged);

    TEST_ASSERT_EQUAL_INT32(1, buf[0].key);
    TEST_ASSERT_EQUAL_INT32(10, buf[0].tag);
    TEST_ASSERT_EQUAL_INT32(2, buf[1].key);
    TEST_ASSERT_EQUAL_INT32(20, buf[1].tag);
    TEST_ASSERT_EQUAL_INT32(3, buf[2].key);
    TEST_ASSERT_EQUAL_INT32(30, buf[2].tag);
}

/* ========== sort_stable ========== */

static void test_sort_stable_orders_a_shuffled_span() {
    int32_t buf[6] = {5, 3, 1, 4, 2, 6};

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_span_sort_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 6), nad_cmp_fn_i32, nad_al_default())
    );

    constexpr int32_t want[6] = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 6);
}

// the reason this variant exists: equal keys must keep their original order
static void test_sort_stable_keeps_equal_elems_in_order() {
    Tagged buf[6] = {
        {2, 0}, {1, 0}, {2, 1}, {1, 1}, {2, 2}, {1, 2},
    };

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_span_sort_stable(NAD_SPAN_NEW_MUT(Tagged, buf, 6), cmp_tagged, nad_al_default())
    );

    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_INT32(1, buf[i].key);
        TEST_ASSERT_EQUAL_INT32((int32_t) i, buf[i].tag);
    }
    for (size_t i = 3; i < 6; ++i) {
        TEST_ASSERT_EQUAL_INT32(2, buf[i].key);
        TEST_ASSERT_EQUAL_INT32((int32_t) (i - 3), buf[i].tag);
    }
}

// the bottom-up merge walks runs of 1, 2, 4, ...; a length that is not a power of two
// leaves a short tail run on every pass, which is where the bounds get interesting
static void test_sort_stable_handles_lengths_that_are_not_powers_of_two() {
    for (size_t len = 1; len <= 11; ++len) {
        int32_t buf[11];
        for (size_t i = 0; i < len; ++i) {
            buf[i] = (int32_t) ((i * 7 + 3) % 11);
        }

        int32_t want[11];
        sorted_copy(want, buf, len);

        TEST_ASSERT_EQUAL_INT(
            NAD_STATUS_OK,
            nad_span_sort_stable(NAD_SPAN_NEW_MUT(int32_t, buf, len), nad_cmp_fn_i32, nad_al_default())
        );

        TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, len);
    }
}

static void test_sort_stable_stays_within_the_subspan() {
    int32_t buf[5] = {9, 3, 1, 2, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_span_sort_stable(nad_span_sub_mut(s, 1, 3), nad_cmp_fn_i32, nad_al_default())
    );

    constexpr int32_t want[5] = {9, 1, 2, 3, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

static void test_sort_stable_works_through_an_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    int32_t buf[5] = {5, 4, 3, 2, 1};

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_span_sort_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 5), nad_cmp_fn_i32, arena)
    );

    constexpr int32_t want[5] = {1, 2, 3, 4, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);

    nad_al_arena_drop(arena);
}

// the scratch buffer cannot be had: reported, and the span is left alone
static void test_sort_stable_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 16);
    TEST_ASSERT_NOT_NULL(arena);

    int32_t buf[8] = {8, 7, 6, 5, 4, 3, 2, 1};

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OUT_OF_MEMORY,
        nad_span_sort_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 8), nad_cmp_fn_i32, arena)
    );

    constexpr int32_t want[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 8);

    nad_al_arena_drop(arena);
}

// nothing to merge means nothing to allocate — an allocator with no room must still do
static void test_sort_stable_of_empty_and_single_needs_no_scratch() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 16);
    TEST_ASSERT_NOT_NULL(arena);

    int32_t buf[1] = {42};

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_span_sort_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 0), nad_cmp_fn_i32, arena)
    );
    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_span_sort_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 1), nad_cmp_fn_i32, arena)
    );

    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
    TEST_ASSERT_EQUAL_size_t(0, nad_al_arena_stats(arena).used);

    nad_al_arena_drop(arena);
}

/* ========== partial_sort ========== */

static void test_partial_sort_puts_the_n_smallest_in_order_at_the_front() {
    int32_t buf[8] = {7, 2, 8, 1, 5, 3, 6, 4};
    constexpr int32_t src[8] = {7, 2, 8, 1, 5, 3, 6, 4};

    nad_span_partial_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 8), 3, nad_cmp_fn_i32);

    constexpr int32_t want_head[3] = {1, 2, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want_head, buf, 3);

    // the tail is whatever is left, but nothing may go missing
    TEST_ASSERT_TRUE(same_elems(src, buf, 8));

    // and nothing smaller than the head may hide in the tail
    for (size_t i = 3; i < 8; ++i) {
        TEST_ASSERT_TRUE(buf[i] > buf[2]);
    }
}

static void test_partial_sort_with_n_zero_is_a_noop() {
    int32_t buf[4] = {4, 3, 2, 1};

    nad_span_partial_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 4), 0, nad_cmp_fn_i32);

    constexpr int32_t want[4] = {4, 3, 2, 1};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 4);
}

static void test_partial_sort_with_n_equal_to_len_sorts_everything() {
    int32_t buf[5] = {3, 5, 1, 4, 2};

    nad_span_partial_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 5), 5, nad_cmp_fn_i32);

    constexpr int32_t want[5] = {1, 2, 3, 4, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

static void test_partial_sort_follows_the_comparator() {
    int32_t buf[6] = {3, 6, 1, 5, 2, 4};

    nad_span_partial_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 6), 2, nad_cmp_fn_desc_i32);

    constexpr int32_t want_head[2] = {6, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want_head, buf, 2);
}

static void test_partial_sort_handles_duplicates() {
    int32_t buf[7] = {3, 1, 3, 1, 2, 3, 2};
    constexpr int32_t src[7] = {3, 1, 3, 1, 2, 3, 2};

    nad_span_partial_sort(NAD_SPAN_NEW_MUT(int32_t, buf, 7), 4, nad_cmp_fn_i32);

    constexpr int32_t want_head[4] = {1, 1, 2, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want_head, buf, 4);
    TEST_ASSERT_TRUE(same_elems(src, buf, 7));
}

/* ========== nth_elem ========== */

// every position in one sweep: whatever lands at nth must be what a full sort would
// have put there
static void test_nth_elem_places_every_position() {
    constexpr int32_t src[7] = {6, 2, 7, 1, 5, 3, 4};

    int32_t want[7];
    sorted_copy(want, src, 7);

    for (size_t nth = 0; nth < 7; ++nth) {
        int32_t buf[7];
        memcpy(buf, src, sizeof buf);

        nad_span_nth_elem(NAD_SPAN_NEW_MUT(int32_t, buf, 7), nth, nad_cmp_fn_i32);

        TEST_ASSERT_EQUAL_INT32(want[nth], buf[nth]);
        TEST_ASSERT_TRUE(same_elems(src, buf, 7));
    }
}

// the point of the operation: the span is split around nth, even though neither side
// is ordered within itself
static void test_nth_elem_partitions_around_the_nth() {
    int32_t buf[9] = {9, 1, 8, 2, 7, 3, 6, 4, 5};
    constexpr size_t nth = 4;

    nad_span_nth_elem(NAD_SPAN_NEW_MUT(int32_t, buf, 9), nth, nad_cmp_fn_i32);

    for (size_t i = 0; i < nth; ++i) {
        TEST_ASSERT_TRUE(buf[i] <= buf[nth]);
    }
    for (size_t i = nth + 1; i < 9; ++i) {
        TEST_ASSERT_TRUE(buf[i] >= buf[nth]);
    }
}

static void test_nth_elem_of_a_single_is_a_noop() {
    int32_t buf[1] = {42};

    nad_span_nth_elem(NAD_SPAN_NEW_MUT(int32_t, buf, 1), 0, nad_cmp_fn_i32);

    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
}

// equal elements make many positions interchangeable; the value at nth is still fixed
static void test_nth_elem_handles_duplicates() {
    constexpr int32_t src[8] = {2, 1, 2, 1, 2, 1, 2, 1};

    int32_t want[8];
    sorted_copy(want, src, 8);

    for (size_t nth = 0; nth < 8; ++nth) {
        int32_t buf[8];
        memcpy(buf, src, sizeof buf);

        nad_span_nth_elem(NAD_SPAN_NEW_MUT(int32_t, buf, 8), nth, nad_cmp_fn_i32);

        TEST_ASSERT_EQUAL_INT32(want[nth], buf[nth]);
    }
}

static void test_nth_elem_stays_within_the_subspan() {
    int32_t buf[6] = {9, 4, 1, 3, 2, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    nad_span_nth_elem(nad_span_sub_mut(s, 1, 4), 0, nad_cmp_fn_i32);

    TEST_ASSERT_EQUAL_INT32(9, buf[0]);
    TEST_ASSERT_EQUAL_INT32(1, buf[1]); // the smallest of the four
    TEST_ASSERT_EQUAL_INT32(9, buf[5]);
}

/* ========== scaling ==========
 *
 * A bad pivot choice still produces the right answer, just after far too many
 * comparisons — no assertion about the output can see it. These count the comparisons
 * instead. 65536 is 2^16, so n * log2(n) is exact and needs no math.h.
 *
 * The size is not arbitrary. Sampling the pivot only at the ends degrades to about
 * n^1.5, which at n = 4096 is still within 4x of n*log n and hides; measured against
 * this implementation it is 4.0x at 4096, 6.6x at 16384 and 11.3x at 65536, against
 * 1.0x here. The counts are fully deterministic, so the margin is real, not statistical.
 */

static constexpr size_t SCALE_N = 65536;
static constexpr size_t SCALE_N_LOG_N = SCALE_N * 16;
static constexpr size_t SCALE_LIMIT = 3 * SCALE_N_LOG_N;

static size_t cmp_calls = 0;

static int cmp_i32_counting(const void *a, const void *b) {
    ++cmp_calls;
    return nad_cmp_fn_i32(a, b);
}

static void test_sort_stays_n_log_n_on_ordered_input() {
    static int32_t buf[SCALE_N];

    for (size_t i = 0; i < SCALE_N; ++i) {
        buf[i] = (int32_t) i;
    }
    cmp_calls = 0;
    nad_span_sort(NAD_SPAN_NEW_MUT(int32_t, buf, SCALE_N), cmp_i32_counting);
    TEST_ASSERT_LESS_THAN_size_t(SCALE_LIMIT, cmp_calls);

    for (size_t i = 0; i < SCALE_N; ++i) {
        buf[i] = (int32_t) (SCALE_N - i);
    }
    cmp_calls = 0;
    nad_span_sort(NAD_SPAN_NEW_MUT(int32_t, buf, SCALE_N), cmp_i32_counting);
    TEST_ASSERT_LESS_THAN_size_t(SCALE_LIMIT, cmp_calls);
}

// a run of equal keys must be settled in one pass, not peeled off one elem at a time
static void test_sort_stays_linear_on_equal_keys() {
    static int32_t buf[SCALE_N];
    for (size_t i = 0; i < SCALE_N; ++i) {
        buf[i] = 7;
    }

    cmp_calls = 0;
    nad_span_sort(NAD_SPAN_NEW_MUT(int32_t, buf, SCALE_N), cmp_i32_counting);

    TEST_ASSERT_LESS_THAN_size_t(8 * SCALE_N, cmp_calls);
}

// nth_elem is expected linear, and shares the pivot choice and the partition with sort
static void test_nth_elem_stays_linear() {
    static int32_t buf[SCALE_N];

    for (size_t i = 0; i < SCALE_N; ++i) {
        buf[i] = (int32_t) i;
    }
    cmp_calls = 0;
    nad_span_nth_elem(NAD_SPAN_NEW_MUT(int32_t, buf, SCALE_N), SCALE_N / 2, cmp_i32_counting);
    TEST_ASSERT_LESS_THAN_size_t(8 * SCALE_N, cmp_calls);

    for (size_t i = 0; i < SCALE_N; ++i) {
        buf[i] = 7;
    }
    cmp_calls = 0;
    nad_span_nth_elem(NAD_SPAN_NEW_MUT(int32_t, buf, SCALE_N), SCALE_N / 2, cmp_i32_counting);
    TEST_ASSERT_LESS_THAN_size_t(8 * SCALE_N, cmp_calls);
}

/* ========== is_sorted ========== */

static void test_is_sorted_accepts_ascending() {
    constexpr int32_t buf[4] = {1, 2, 3, 4};

    TEST_ASSERT_TRUE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 4), nad_cmp_fn_i32));
}

static void test_is_sorted_allows_equal_neighbours() {
    constexpr int32_t buf[4] = {1, 2, 2, 3};

    TEST_ASSERT_TRUE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 4), nad_cmp_fn_i32));
}

// the break of order is in the last pair — the walk must reach it
static void test_is_sorted_rejects_a_late_inversion() {
    constexpr int32_t buf[4] = {1, 2, 3, 0};

    TEST_ASSERT_FALSE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 4), nad_cmp_fn_i32));
}

static void test_is_sorted_empty_and_single_are_sorted() {
    constexpr int32_t buf[1] = {42};

    TEST_ASSERT_TRUE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 0), nad_cmp_fn_i32));
    TEST_ASSERT_TRUE(nad_span_is_sorted(NAD_SPAN_NEW(int32_t, buf, 1), nad_cmp_fn_i32));
}

static void test_is_sorted_agrees_with_sort() {
    int32_t buf[6] = {4, 1, 6, 2, 5, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    TEST_ASSERT_FALSE(nad_span_is_sorted(nad_span_from_mut(s), nad_cmp_fn_i32));
    nad_span_insertion_sort(s, nad_cmp_fn_i32);
    TEST_ASSERT_TRUE(nad_span_is_sorted(nad_span_from_mut(s), nad_cmp_fn_i32));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_insertion_sort_orders_a_shuffled_span);
    RUN_TEST(test_insertion_sort_already_sorted_is_unchanged);
    RUN_TEST(test_insertion_sort_reversed_span);
    RUN_TEST(test_insertion_sort_keeps_duplicates);
    RUN_TEST(test_insertion_sort_empty_and_single_are_noop);
    RUN_TEST(test_insertion_sort_follows_the_comparator);
    RUN_TEST(test_insertion_sort_is_stable);
    RUN_TEST(test_insertion_sort_stays_within_the_subspan);

    RUN_TEST(test_sort_orders_a_shuffled_span);
    RUN_TEST(test_sort_keeps_duplicates);
    RUN_TEST(test_sort_empty_and_single_are_noop);
    RUN_TEST(test_sort_follows_the_comparator);
    RUN_TEST(test_sort_stays_within_the_subspan);
    RUN_TEST(test_sort_orders_a_long_span);
    RUN_TEST(test_sort_moves_whole_elems);

    RUN_TEST(test_sort_stable_orders_a_shuffled_span);
    RUN_TEST(test_sort_stable_keeps_equal_elems_in_order);
    RUN_TEST(test_sort_stable_handles_lengths_that_are_not_powers_of_two);
    RUN_TEST(test_sort_stable_stays_within_the_subspan);
    RUN_TEST(test_sort_stable_works_through_an_arena);
    RUN_TEST(test_sort_stable_reports_an_exhausted_arena);
    RUN_TEST(test_sort_stable_of_empty_and_single_needs_no_scratch);

    RUN_TEST(test_partial_sort_puts_the_n_smallest_in_order_at_the_front);
    RUN_TEST(test_partial_sort_with_n_zero_is_a_noop);
    RUN_TEST(test_partial_sort_with_n_equal_to_len_sorts_everything);
    RUN_TEST(test_partial_sort_follows_the_comparator);
    RUN_TEST(test_partial_sort_handles_duplicates);

    RUN_TEST(test_nth_elem_places_every_position);
    RUN_TEST(test_nth_elem_partitions_around_the_nth);
    RUN_TEST(test_nth_elem_of_a_single_is_a_noop);
    RUN_TEST(test_nth_elem_handles_duplicates);
    RUN_TEST(test_nth_elem_stays_within_the_subspan);

    RUN_TEST(test_sort_stays_n_log_n_on_ordered_input);
    RUN_TEST(test_sort_stays_linear_on_equal_keys);
    RUN_TEST(test_nth_elem_stays_linear);

    RUN_TEST(test_is_sorted_accepts_ascending);
    RUN_TEST(test_is_sorted_allows_equal_neighbours);
    RUN_TEST(test_is_sorted_rejects_a_late_inversion);
    RUN_TEST(test_is_sorted_empty_and_single_are_sorted);
    RUN_TEST(test_is_sorted_agrees_with_sort);

    return UNITY_END();
}
