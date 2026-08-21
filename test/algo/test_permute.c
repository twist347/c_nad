#include "nad/algo/permute.h"
#include "nad/algo/search.h"
#include "nad/alloc/default.h"
#include "nad/core/util.h"

#include "support/pair.h"
#include "support/probe.h"
#include "support/status.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp() {
}

void tearDown() {
}

static bool is_even(const void *elem, void *ctx) {
    NAD_UNUSED(ctx);

    return *(const int32_t *) elem % 2 == 0;
}

static bool greater_than(const void *elem, void *ctx) {
    return *(const int32_t *) elem > *(const int32_t *) ctx;
}

// the same question as is_even, but it keeps a tally in ctx: how many times a
// partition asks about an elem is part of its contract
static bool is_even_counting(const void *elem, void *ctx) {
    ++*(size_t *) ctx;

    return *(const int32_t *) elem % 2 == 0;
}

// the multiset must survive a permutation, whatever the order
static void assert_same_elems(const int32_t *got, const int32_t *want, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        size_t got_count = 0;
        size_t want_count = 0;
        for (size_t j = 0; j < n; ++j) {
            got_count += got[j] == want[i];
            want_count += want[j] == want[i];
        }
        TEST_ASSERT_EQUAL_size_t(want_count, got_count);
    }
}

/* ========== reverse ========== */

static void test_reverse_turns_the_span_around() {
    int32_t buf[5] = {1, 2, 3, 4, 5};

    nad_span_reverse(NAD_SPAN_NEW_MUT(int32_t, buf, 5));

    constexpr int32_t want[5] = {5, 4, 3, 2, 1};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

// an odd length leaves the middle elem where it was
static void test_reverse_of_an_even_length_moves_every_elem() {
    int32_t buf[4] = {1, 2, 3, 4};

    nad_span_reverse(NAD_SPAN_NEW_MUT(int32_t, buf, 4));

    constexpr int32_t want[4] = {4, 3, 2, 1};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 4);
}

static void test_reverse_empty_and_single_are_noop() {
    int32_t buf[1] = {42};

    nad_span_reverse(NAD_SPAN_NEW_MUT(int32_t, buf, 0));
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);

    nad_span_reverse(NAD_SPAN_NEW_MUT(int32_t, buf, 1));
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
}

static void test_reverse_twice_is_the_identity() {
    int32_t buf[5] = {3, 1, 4, 1, 5};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_reverse(s);
    nad_span_reverse(s);

    constexpr int32_t want[5] = {3, 1, 4, 1, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

static void test_reverse_stays_within_the_subspan() {
    int32_t buf[5] = {9, 1, 2, 3, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_reverse(nad_span_sub_mut(s, 1, 3));

    constexpr int32_t want[5] = {9, 3, 2, 1, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

// elem_size drives the swap, so a type wider than a word must move whole
static void test_reverse_moves_whole_elems() {
    Pair buf[3] = {{1, 2}, {3, 4}, {5, 6}};

    nad_span_reverse(NAD_SPAN_NEW_MUT(Pair, buf, 3));

    TEST_ASSERT_EQUAL_INT64(5, buf[0].a);
    TEST_ASSERT_EQUAL_INT64(6, buf[0].b);
    TEST_ASSERT_EQUAL_INT64(3, buf[1].a);
    TEST_ASSERT_EQUAL_INT64(4, buf[1].b);
    TEST_ASSERT_EQUAL_INT64(1, buf[2].a);
    TEST_ASSERT_EQUAL_INT64(2, buf[2].b);
}

/* ========== rotate ========== */

// every offset in one sweep: rotate(mid) must leave elem i at (i + mid) % len.
// The bug this pins down was right only at mid == 1 and ran off the buffer past that.
static void test_rotate_by_every_offset() {
    for (size_t mid = 0; mid <= 5; ++mid) {
        int32_t buf[5] = {0, 1, 2, 3, 4};
        int32_t want[5];
        for (size_t i = 0; i < 5; ++i) {
            want[i] = (int32_t) ((i + mid) % 5);
        }

        nad_span_rotate(NAD_SPAN_NEW_MUT(int32_t, buf, 5), mid);

        TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
    }
}

static void test_rotate_by_zero_and_by_len_are_noop() {
    int32_t buf[4] = {1, 2, 3, 4};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 4);
    constexpr int32_t want[4] = {1, 2, 3, 4};

    nad_span_rotate(s, 0);
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 4);

    nad_span_rotate(s, 4);
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 4);
}

// rotating by mid and then by len - mid comes back to the start
static void test_rotate_composes_back_to_the_identity() {
    int32_t buf[6] = {1, 2, 3, 4, 5, 6};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    nad_span_rotate(s, 2);
    nad_span_rotate(s, 4);

    constexpr int32_t want[6] = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 6);
}

static void test_rotate_stays_within_the_subspan() {
    int32_t buf[6] = {9, 1, 2, 3, 4, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    nad_span_rotate(nad_span_sub_mut(s, 1, 4), 1);

    constexpr int32_t want[6] = {9, 2, 3, 4, 1, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 6);
}

static void test_rotate_moves_whole_elems() {
    Pair buf[3] = {{1, 2}, {3, 4}, {5, 6}};

    nad_span_rotate(NAD_SPAN_NEW_MUT(Pair, buf, 3), 1);

    TEST_ASSERT_EQUAL_INT64(3, buf[0].a);
    TEST_ASSERT_EQUAL_INT64(4, buf[0].b);
    TEST_ASSERT_EQUAL_INT64(5, buf[1].a);
    TEST_ASSERT_EQUAL_INT64(6, buf[1].b);
    TEST_ASSERT_EQUAL_INT64(1, buf[2].a);
    TEST_ASSERT_EQUAL_INT64(2, buf[2].b);
}

/* ========== swap_ranges ========== */

static void test_swap_ranges_exchanges_the_two_views() {
    int32_t a[3] = {1, 2, 3};
    int32_t b[3] = {7, 8, 9};

    nad_span_swap_ranges(NAD_SPAN_NEW_MUT(int32_t, a, 3), NAD_SPAN_NEW_MUT(int32_t, b, 3));

    constexpr int32_t want_a[3] = {7, 8, 9};
    constexpr int32_t want_b[3] = {1, 2, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want_a, a, 3);
    TEST_ASSERT_EQUAL_INT32_ARRAY(want_b, b, 3);
}

// two halves of one buffer are legal as long as they do not overlap
static void test_swap_ranges_works_on_halves_of_one_buffer() {
    int32_t buf[6] = {1, 2, 3, 4, 5, 6};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    nad_span_swap_ranges(nad_span_sub_mut(s, 0, 3), nad_span_sub_mut(s, 3, 3));

    constexpr int32_t want[6] = {4, 5, 6, 1, 2, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 6);
}

static void test_swap_ranges_empty_is_noop() {
    int32_t a[1] = {1};
    int32_t b[1] = {2};

    nad_span_swap_ranges(NAD_SPAN_NEW_MUT(int32_t, a, 0), NAD_SPAN_NEW_MUT(int32_t, b, 0));

    TEST_ASSERT_EQUAL_INT32(1, a[0]);
    TEST_ASSERT_EQUAL_INT32(2, b[0]);
}

// the same view on both sides would swap every elem with itself — short-circuited
static void test_swap_ranges_of_one_view_with_itself_is_noop() {
    int32_t buf[3] = {1, 2, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    nad_span_swap_ranges(s, s);

    constexpr int32_t want[3] = {1, 2, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 3);
}

static void test_swap_ranges_moves_whole_elems() {
    Pair a[2] = {{1, 2}, {3, 4}};
    Pair b[2] = {{5, 6}, {7, 8}};

    nad_span_swap_ranges(NAD_SPAN_NEW_MUT(Pair, a, 2), NAD_SPAN_NEW_MUT(Pair, b, 2));

    TEST_ASSERT_EQUAL_INT64(5, a[0].a);
    TEST_ASSERT_EQUAL_INT64(8, a[1].b);
    TEST_ASSERT_EQUAL_INT64(1, b[0].a);
    TEST_ASSERT_EQUAL_INT64(4, b[1].b);
}

/* ========== next_permutation ========== */

// the six permutations of {1,2,3} in lexicographic order
static const int32_t ALL_3[6][3] = {
    {1, 2, 3}, {1, 3, 2}, {2, 1, 3}, {2, 3, 1}, {3, 1, 2}, {3, 2, 1},
};

// a full sweep: every step must land on the next permutation, and only the last
// one may report false
static void test_next_permutation_walks_the_whole_order() {
    int32_t buf[3] = {1, 2, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    for (size_t i = 0; i < 6; ++i) {
        TEST_ASSERT_EQUAL_INT32_ARRAY(ALL_3[i], buf, 3);

        const bool more = nad_span_next_permutation(s, nad_cmp_i32);
        if (i < 5) {
            TEST_ASSERT_TRUE(more);
        } else {
            TEST_ASSERT_FALSE(more);
        }
    }

    // reporting false also resets to the first permutation
    TEST_ASSERT_EQUAL_INT32_ARRAY(ALL_3[0], buf, 3);
}

// equal elements do not produce equal permutations twice: {1,1,2} has three, not six
static void test_next_permutation_skips_duplicate_arrangements() {
    int32_t buf[3] = {1, 1, 2};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    size_t seen = 1;
    while (nad_span_next_permutation(s, nad_cmp_i32)) {
        ++seen;
    }

    TEST_ASSERT_EQUAL_size_t(3, seen);
}

static void test_next_permutation_empty_and_single_report_false() {
    int32_t buf[1] = {42};

    TEST_ASSERT_FALSE(nad_span_next_permutation(NAD_SPAN_NEW_MUT(int32_t, buf, 0), nad_cmp_i32));
    TEST_ASSERT_FALSE(nad_span_next_permutation(NAD_SPAN_NEW_MUT(int32_t, buf, 1), nad_cmp_i32));
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
}

// the comparator defines the order, so a descending one walks the mirror sequence
static void test_next_permutation_follows_the_comparator() {
    int32_t buf[3] = {3, 2, 1};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    TEST_ASSERT_TRUE(nad_span_next_permutation(s, nad_cmp_desc_i32));

    constexpr int32_t want[3] = {3, 1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 3);
}

static void test_next_permutation_stays_within_the_subspan() {
    int32_t buf[5] = {9, 1, 2, 3, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    TEST_ASSERT_TRUE(nad_span_next_permutation(nad_span_sub_mut(s, 1, 3), nad_cmp_i32));

    constexpr int32_t want[5] = {9, 1, 3, 2, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

/* ========== prev_permutation ========== */

static void test_prev_permutation_walks_the_order_backwards() {
    int32_t buf[3] = {3, 2, 1};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    for (size_t i = 6; i > 0; --i) {
        TEST_ASSERT_EQUAL_INT32_ARRAY(ALL_3[i - 1], buf, 3);

        const bool more = nad_span_prev_permutation(s, nad_cmp_i32);
        if (i > 1) {
            TEST_ASSERT_TRUE(more);
        } else {
            TEST_ASSERT_FALSE(more);
        }
    }

    // reporting false also resets to the last permutation
    TEST_ASSERT_EQUAL_INT32_ARRAY(ALL_3[5], buf, 3);
}

// the two are inverses of each other on every step that is not an end
static void test_prev_permutation_undoes_next_permutation() {
    for (size_t i = 0; i < 5; ++i) {
        int32_t buf[3];
        memcpy(buf, ALL_3[i], sizeof buf);
        const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

        TEST_ASSERT_TRUE(nad_span_next_permutation(s, nad_cmp_i32));
        TEST_ASSERT_TRUE(nad_span_prev_permutation(s, nad_cmp_i32));

        TEST_ASSERT_EQUAL_INT32_ARRAY(ALL_3[i], buf, 3);
    }
}

static void test_prev_permutation_empty_and_single_report_false() {
    int32_t buf[1] = {42};

    TEST_ASSERT_FALSE(nad_span_prev_permutation(NAD_SPAN_NEW_MUT(int32_t, buf, 0), nad_cmp_i32));
    TEST_ASSERT_FALSE(nad_span_prev_permutation(NAD_SPAN_NEW_MUT(int32_t, buf, 1), nad_cmp_i32));
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
}

static void test_prev_permutation_moves_whole_elems() {
    Pair buf[2] = {{3, 30}, {1, 10}};

    TEST_ASSERT_TRUE(nad_span_prev_permutation(NAD_SPAN_NEW_MUT(Pair, buf, 2), nad_cmp_i32));

    // nad_cmp_i32 reads the first int32 of each Pair, but the whole elem must travel
    TEST_ASSERT_EQUAL_INT64(1, buf[0].a);
    TEST_ASSERT_EQUAL_INT64(10, buf[0].b);
    TEST_ASSERT_EQUAL_INT64(3, buf[1].a);
    TEST_ASSERT_EQUAL_INT64(30, buf[1].b);
}

/* ========== partition ========== */

static void test_partition_moves_matches_to_the_front() {
    int32_t buf[6] = {1, 2, 3, 4, 5, 6};

    const size_t boundary = nad_span_partition(NAD_SPAN_NEW_MUT(int32_t, buf, 6), is_even, nullptr);

    TEST_ASSERT_EQUAL_size_t(3, boundary);
    for (size_t i = 0; i < boundary; ++i) {
        TEST_ASSERT_TRUE(buf[i] % 2 == 0);
    }
    for (size_t i = boundary; i < 6; ++i) {
        TEST_ASSERT_FALSE(buf[i] % 2 == 0);
    }

    constexpr int32_t want[6] = {1, 2, 3, 4, 5, 6};
    assert_same_elems(buf, want, 6);
}

static void test_partition_at_the_ends() {
    int32_t all[3] = {2, 4, 6};
    int32_t none[3] = {1, 3, 5};

    TEST_ASSERT_EQUAL_size_t(3, nad_span_partition(NAD_SPAN_NEW_MUT(int32_t, all, 3), is_even, nullptr));
    TEST_ASSERT_EQUAL_size_t(0, nad_span_partition(NAD_SPAN_NEW_MUT(int32_t, none, 3), is_even, nullptr));

    constexpr int32_t want_all[3] = {2, 4, 6};
    constexpr int32_t want_none[3] = {1, 3, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want_all, all, 3);
    TEST_ASSERT_EQUAL_INT32_ARRAY(want_none, none, 3);
}

static void test_partition_of_an_empty_span_is_zero() {
    TEST_ASSERT_EQUAL_size_t(0, nad_span_partition(NAD_SPAN_NEW_MUT(int32_t, nullptr, 0), is_even, nullptr));
}

static void test_partition_passes_the_ctx_through() {
    int32_t buf[5] = {1, 5, 2, 4, 3};
    int32_t bound = 3;

    const size_t boundary = nad_span_partition(NAD_SPAN_NEW_MUT(int32_t, buf, 5), greater_than, &bound);

    TEST_ASSERT_EQUAL_size_t(2, boundary);
    for (size_t i = 0; i < boundary; ++i) {
        TEST_ASSERT_TRUE(buf[i] > bound);
    }

    constexpr int32_t want[5] = {1, 2, 3, 4, 5};
    assert_same_elems(buf, want, 5);
}

static void test_partition_moves_whole_elems() {
    Pair buf[4] = {{-1, 10}, {1, 20}, {-2, 30}, {2, 40}};

    const size_t boundary = nad_span_partition(NAD_SPAN_NEW_MUT(Pair, buf, 4), nad_test_pair_a_is_positive, nullptr);

    TEST_ASSERT_EQUAL_size_t(2, boundary);
    for (size_t i = 0; i < boundary; ++i) {
        TEST_ASSERT_TRUE(buf[i].a > 0);
        TEST_ASSERT_EQUAL_INT64(buf[i].a == 1 ? 20 : 40, buf[i].b); // b travelled with a
    }
}

/* ========== partition_stable ========== */

static void test_partition_stable_keeps_the_order_on_both_sides() {
    int32_t buf[6] = {1, 2, 3, 4, 5, 6};

    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 6), is_even, nullptr,
                                          nad_al_default(), &boundary));

    constexpr int32_t want[6] = {2, 4, 6, 1, 3, 5};
    TEST_ASSERT_EQUAL_size_t(3, boundary);
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 6);
}

// the two partitions disagree about the order inside each side, never about where
// the sides meet — same span, same pred, same boundary
static void test_partition_stable_agrees_with_partition_on_the_boundary() {
    int32_t stable_buf[7] = {4, 1, 6, 3, 8, 5, 2};
    int32_t plain_buf[7] = {4, 1, 6, 3, 8, 5, 2};

    size_t stable = 999;
    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, stable_buf, 7), is_even,
                                          nullptr, nad_al_default(), &stable));

    const size_t plain = nad_span_partition(NAD_SPAN_NEW_MUT(int32_t, plain_buf, 7), is_even, nullptr);

    TEST_ASSERT_EQUAL_size_t(plain, stable);
    assert_same_elems(stable_buf, plain_buf, 7);
}

// the whole point: elems the pred cannot tell apart come out in the order they went in.
// The tag in b witnesses it — nothing else here can
static void test_partition_stable_keeps_equal_elems_in_order() {
    Pair buf[6] = {{1, 0}, {-1, 1}, {1, 2}, {-1, 3}, {-1, 4}, {1, 5}};

    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(Pair, buf, 6),
                                          nad_test_pair_a_is_positive, nullptr,
                                          nad_al_default(), &boundary));

    TEST_ASSERT_EQUAL_size_t(3, boundary);
    constexpr int64_t want_tags[6] = {0, 2, 5, 1, 3, 4};
    for (size_t i = 0; i < 6; ++i) {
        TEST_ASSERT_EQUAL_INT64(want_tags[i], buf[i].b);
    }
}

// a span already split by the pred is its own stable partition, byte for byte
static void test_partition_stable_leaves_a_partitioned_span_alone() {
    int32_t buf[5] = {2, 4, 1, 3, 5};

    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 5), is_even, nullptr,
                                          nad_al_default(), &boundary));

    constexpr int32_t want[5] = {2, 4, 1, 3, 5};
    TEST_ASSERT_EQUAL_size_t(2, boundary);
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

static void test_partition_stable_at_the_ends() {
    int32_t all[3] = {2, 4, 6};
    int32_t none[3] = {1, 3, 5};

    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, all, 3), is_even, nullptr,
                                          nad_al_default(), &boundary));
    TEST_ASSERT_EQUAL_size_t(3, boundary);
    TEST_ASSERT_EQUAL_INT32_ARRAY(((int32_t[]){2, 4, 6}), all, 3);

    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, none, 3), is_even, nullptr,
                                          nad_al_default(), &boundary));
    TEST_ASSERT_EQUAL_size_t(0, boundary);
    TEST_ASSERT_EQUAL_INT32_ARRAY(((int32_t[]){1, 3, 5}), none, 3);
}

static void test_partition_stable_result_is_partitioned() {
    int32_t buf[8] = {7, 2, 9, 4, 1, 6, 3, 8};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 8);

    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(s, is_even, nullptr, nad_al_default(), &boundary));

    TEST_ASSERT_TRUE(nad_span_is_partitioned(nad_span_mut_to_span(s), is_even, nullptr));
    TEST_ASSERT_EQUAL_size_t(nad_span_partition_point(nad_span_mut_to_span(s), is_even, nullptr),
                             boundary);
}

static void test_partition_stable_passes_the_ctx_through() {
    int32_t buf[5] = {1, 5, 2, 4, 3};
    constexpr int32_t bound = 2;

    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 5), greater_than,
                                          (void *) &bound, nad_al_default(), &boundary));

    constexpr int32_t want[5] = {5, 4, 3, 1, 2};
    TEST_ASSERT_EQUAL_size_t(3, boundary);
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

// only a subspan is split, and the elems around it must not move
static void test_partition_stable_of_a_subspan_leaves_the_neighbours_alone() {
    int32_t buf[6] = {9, 1, 2, 3, 4, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 6);

    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(nad_span_sub_mut(s, 1, 4), is_even, nullptr,
                                          nad_al_default(), &boundary));

    constexpr int32_t want[6] = {9, 2, 4, 1, 3, 9};
    TEST_ASSERT_EQUAL_size_t(2, boundary);
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 6);
}

// asking twice would be a second, differently timed set of answers — the contract
// says once per elem, so count them
static void test_partition_stable_asks_the_pred_once_per_elem() {
    int32_t buf[6] = {1, 2, 3, 4, 5, 6};

    size_t asked = 0;
    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 6), is_even_counting,
                                          &asked, nad_al_default(), &boundary));

    TEST_ASSERT_EQUAL_size_t(6, asked);
}

// the scratch is borrowed for the call and returned by the end of it
static void test_partition_stable_releases_its_scratch() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    int32_t buf[4] = {1, 2, 3, 4};

    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 4), is_even, nullptr,
                                          &al, &boundary));

    TEST_ASSERT_EQUAL_size_t(1, probe.alloc_calls);
    TEST_ASSERT_EQUAL_size_t(1, probe.dealloc_calls);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
    TEST_ASSERT_EQUAL_size_t(4 * sizeof(int32_t), probe.last_alloc_size);
    TEST_ASSERT_EQUAL_size_t(probe.last_alloc_size, probe.last_dealloc_size);
}

// no scratch, no split: the span keeps the order it had and the boundary is not written
static void test_partition_stable_reports_a_refused_scratch() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);
    nad_test_probe_fail_after_next(&probe, 0);

    int32_t buf[4] = {1, 2, 3, 4};

    size_t boundary = 777;
    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, buf, 4), is_even, nullptr, &al, &boundary)
    );

    constexpr int32_t want[4] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 4);
    TEST_ASSERT_EQUAL_size_t(777, boundary);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// an empty span has nothing to move, so it must not need memory it cannot get
static void test_partition_stable_of_an_empty_span_asks_for_nothing() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);
    nad_test_probe_fail_after_next(&probe, 0);

    size_t boundary = 999;
    NAD_TEST_OK(nad_span_partition_stable(NAD_SPAN_NEW_MUT(int32_t, nullptr, 0), is_even,
                                          nullptr, &al, &boundary));

    TEST_ASSERT_EQUAL_size_t(0, boundary);
    TEST_ASSERT_EQUAL_size_t(0, nad_test_probe_requests(&probe));
}

/* ========== is_partitioned ========== */

static void test_is_partitioned_accepts_a_split_span() {
    constexpr int32_t split[5] = {2, 4, 1, 3, 5};
    constexpr int32_t mixed[5] = {2, 1, 4, 3, 5};

    TEST_ASSERT_TRUE(nad_span_is_partitioned(NAD_SPAN_NEW(int32_t, split, 5), is_even, nullptr));
    TEST_ASSERT_FALSE(nad_span_is_partitioned(NAD_SPAN_NEW(int32_t, mixed, 5), is_even, nullptr));
}

static void test_is_partitioned_on_uniform_and_empty_spans() {
    constexpr int32_t all[3] = {2, 4, 6};
    constexpr int32_t none[3] = {1, 3, 5};

    TEST_ASSERT_TRUE(nad_span_is_partitioned(NAD_SPAN_NEW(int32_t, all, 3), is_even, nullptr));
    TEST_ASSERT_TRUE(nad_span_is_partitioned(NAD_SPAN_NEW(int32_t, none, 3), is_even, nullptr));
    TEST_ASSERT_TRUE(nad_span_is_partitioned(NAD_SPAN_NEW(int32_t, nullptr, 0), is_even, nullptr));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_reverse_turns_the_span_around);
    RUN_TEST(test_reverse_of_an_even_length_moves_every_elem);
    RUN_TEST(test_reverse_empty_and_single_are_noop);
    RUN_TEST(test_reverse_twice_is_the_identity);
    RUN_TEST(test_reverse_stays_within_the_subspan);
    RUN_TEST(test_reverse_moves_whole_elems);

    RUN_TEST(test_rotate_by_every_offset);
    RUN_TEST(test_rotate_by_zero_and_by_len_are_noop);
    RUN_TEST(test_rotate_composes_back_to_the_identity);
    RUN_TEST(test_rotate_stays_within_the_subspan);
    RUN_TEST(test_rotate_moves_whole_elems);

    RUN_TEST(test_swap_ranges_exchanges_the_two_views);
    RUN_TEST(test_swap_ranges_works_on_halves_of_one_buffer);
    RUN_TEST(test_swap_ranges_empty_is_noop);
    RUN_TEST(test_swap_ranges_of_one_view_with_itself_is_noop);
    RUN_TEST(test_swap_ranges_moves_whole_elems);

    RUN_TEST(test_next_permutation_walks_the_whole_order);
    RUN_TEST(test_next_permutation_skips_duplicate_arrangements);
    RUN_TEST(test_next_permutation_empty_and_single_report_false);
    RUN_TEST(test_next_permutation_follows_the_comparator);
    RUN_TEST(test_next_permutation_stays_within_the_subspan);

    RUN_TEST(test_prev_permutation_walks_the_order_backwards);
    RUN_TEST(test_prev_permutation_undoes_next_permutation);
    RUN_TEST(test_prev_permutation_empty_and_single_report_false);
    RUN_TEST(test_prev_permutation_moves_whole_elems);

    RUN_TEST(test_partition_moves_matches_to_the_front);
    RUN_TEST(test_partition_at_the_ends);
    RUN_TEST(test_partition_of_an_empty_span_is_zero);
    RUN_TEST(test_partition_passes_the_ctx_through);
    RUN_TEST(test_partition_moves_whole_elems);

    RUN_TEST(test_partition_stable_keeps_the_order_on_both_sides);
    RUN_TEST(test_partition_stable_agrees_with_partition_on_the_boundary);
    RUN_TEST(test_partition_stable_keeps_equal_elems_in_order);
    RUN_TEST(test_partition_stable_leaves_a_partitioned_span_alone);
    RUN_TEST(test_partition_stable_at_the_ends);
    RUN_TEST(test_partition_stable_result_is_partitioned);
    RUN_TEST(test_partition_stable_passes_the_ctx_through);
    RUN_TEST(test_partition_stable_of_a_subspan_leaves_the_neighbours_alone);
    RUN_TEST(test_partition_stable_asks_the_pred_once_per_elem);
    RUN_TEST(test_partition_stable_releases_its_scratch);
    RUN_TEST(test_partition_stable_reports_a_refused_scratch);
    RUN_TEST(test_partition_stable_of_an_empty_span_asks_for_nothing);

    RUN_TEST(test_is_partitioned_accepts_a_split_span);
    RUN_TEST(test_is_partitioned_on_uniform_and_empty_spans);

    return UNITY_END();
}
