#include "nad/algo/permute.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp() {
}

void tearDown() {
}

// an elem wider than a word, to keep elem_size honest
typedef struct {
    int64_t a;
    int64_t b;
} Pair;

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

        const bool more = nad_span_next_permutation(s, nad_cmp_fn_i32);
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
    while (nad_span_next_permutation(s, nad_cmp_fn_i32)) {
        ++seen;
    }

    TEST_ASSERT_EQUAL_size_t(3, seen);
}

static void test_next_permutation_empty_and_single_report_false() {
    int32_t buf[1] = {42};

    TEST_ASSERT_FALSE(nad_span_next_permutation(NAD_SPAN_NEW_MUT(int32_t, buf, 0), nad_cmp_fn_i32));
    TEST_ASSERT_FALSE(nad_span_next_permutation(NAD_SPAN_NEW_MUT(int32_t, buf, 1), nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
}

// the comparator defines the order, so a descending one walks the mirror sequence
static void test_next_permutation_follows_the_comparator() {
    int32_t buf[3] = {3, 2, 1};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    TEST_ASSERT_TRUE(nad_span_next_permutation(s, nad_cmp_fn_desc_i32));

    constexpr int32_t want[3] = {3, 1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 3);
}

static void test_next_permutation_stays_within_the_subspan() {
    int32_t buf[5] = {9, 1, 2, 3, 9};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    TEST_ASSERT_TRUE(nad_span_next_permutation(nad_span_sub_mut(s, 1, 3), nad_cmp_fn_i32));

    constexpr int32_t want[5] = {9, 1, 3, 2, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, buf, 5);
}

/* ========== prev_permutation ========== */

static void test_prev_permutation_walks_the_order_backwards() {
    int32_t buf[3] = {3, 2, 1};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    for (size_t i = 6; i > 0; --i) {
        TEST_ASSERT_EQUAL_INT32_ARRAY(ALL_3[i - 1], buf, 3);

        const bool more = nad_span_prev_permutation(s, nad_cmp_fn_i32);
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

        TEST_ASSERT_TRUE(nad_span_next_permutation(s, nad_cmp_fn_i32));
        TEST_ASSERT_TRUE(nad_span_prev_permutation(s, nad_cmp_fn_i32));

        TEST_ASSERT_EQUAL_INT32_ARRAY(ALL_3[i], buf, 3);
    }
}

static void test_prev_permutation_empty_and_single_report_false() {
    int32_t buf[1] = {42};

    TEST_ASSERT_FALSE(nad_span_prev_permutation(NAD_SPAN_NEW_MUT(int32_t, buf, 0), nad_cmp_fn_i32));
    TEST_ASSERT_FALSE(nad_span_prev_permutation(NAD_SPAN_NEW_MUT(int32_t, buf, 1), nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_INT32(42, buf[0]);
}

static void test_prev_permutation_moves_whole_elems() {
    Pair buf[2] = {{3, 30}, {1, 10}};

    TEST_ASSERT_TRUE(nad_span_prev_permutation(NAD_SPAN_NEW_MUT(Pair, buf, 2), nad_cmp_fn_i32));

    // nad_cmp_fn_i32 reads the first int32 of each Pair, but the whole elem must travel
    TEST_ASSERT_EQUAL_INT64(1, buf[0].a);
    TEST_ASSERT_EQUAL_INT64(10, buf[0].b);
    TEST_ASSERT_EQUAL_INT64(3, buf[1].a);
    TEST_ASSERT_EQUAL_INT64(30, buf[1].b);
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

    return UNITY_END();
}
