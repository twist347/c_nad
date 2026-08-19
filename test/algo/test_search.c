#include "nad/algo/search.h"
#include "nad/core/util.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// ordered by key only, so tag can witness which of the equal elements was picked
typedef struct {
    int32_t key;
    int32_t tag;
} Tagged;

static int cmp_tagged(const void *a, const void *b) {
    return nad_cmp_i32(((const Tagged *) a)->key, ((const Tagged *) b)->key);
}

// a predicate that needs nothing: ctx stays null
static bool is_even(const void *elem, void *ctx) {
    NAD_UNUSED(ctx);

    return *(const int32_t *) elem % 2 == 0;
}

// a predicate parameterized through ctx — the whole point of carrying one
static bool greater_than(const void *elem, void *ctx) {
    return *(const int32_t *) elem > *(const int32_t *) ctx;
}

/* ========== find ========== */

static void test_find_reports_the_first_match() {
    constexpr int32_t buf[5] = {3, 1, 2, 1, 4};
    constexpr int32_t key = 1;

    size_t idx = 999;
    TEST_ASSERT_TRUE(nad_span_find(NAD_SPAN_NEW(int32_t, buf, 5), &key, nad_eq_fn_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(1, idx);
}

// on a miss the out param is left untouched — the caller's value must survive
static void test_find_miss_leaves_the_out_param_alone() {
    constexpr int32_t buf[3] = {1, 2, 3};
    constexpr int32_t key = 9;

    size_t idx = 777;
    TEST_ASSERT_FALSE(nad_span_find(NAD_SPAN_NEW(int32_t, buf, 3), &key, nad_eq_fn_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(777, idx);
}

static void test_find_in_an_empty_span_misses() {
    constexpr int32_t buf[2] = {1, 2};
    constexpr int32_t key = 1;

    size_t idx = 555;
    TEST_ASSERT_FALSE(nad_span_find(NAD_SPAN_NEW(int32_t, buf, 0), &key, nad_eq_fn_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(555, idx);
}

static void test_find_matches_the_last_elem() {
    constexpr int32_t buf[4] = {1, 2, 3, 4};
    constexpr int32_t key = 4;

    size_t idx = 0;
    TEST_ASSERT_TRUE(nad_span_find(NAD_SPAN_NEW(int32_t, buf, 4), &key, nad_eq_fn_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(3, idx);
}

// searching a subspan reports an index relative to that subspan
static void test_find_index_is_relative_to_the_span() {
    constexpr int32_t buf[5] = {1, 2, 3, 4, 5};
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 5);
    constexpr int32_t key = 4;

    size_t idx = 0;
    TEST_ASSERT_TRUE(nad_span_find(nad_span_sub(s, 2, 3), &key, nad_eq_fn_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(1, idx);
}

/* ========== contains / count ========== */

static void test_contains() {
    constexpr int32_t buf[3] = {1, 2, 3};
    constexpr int32_t present = 2;
    constexpr int32_t absent = 9;

    TEST_ASSERT_TRUE(nad_span_contains(NAD_SPAN_NEW(int32_t, buf, 3), &present, nad_eq_fn_i32));
    TEST_ASSERT_FALSE(nad_span_contains(NAD_SPAN_NEW(int32_t, buf, 3), &absent, nad_eq_fn_i32));
}

static void test_count_tallies_every_match() {
    constexpr int32_t buf[6] = {1, 2, 1, 3, 1, 4};
    constexpr int32_t key = 1;

    TEST_ASSERT_EQUAL_size_t(3, nad_span_count(NAD_SPAN_NEW(int32_t, buf, 6), &key, nad_eq_fn_i32));
}

static void test_count_of_an_absent_key_is_zero() {
    constexpr int32_t buf[3] = {1, 2, 3};
    constexpr int32_t key = 9;

    TEST_ASSERT_EQUAL_size_t(0, nad_span_count(NAD_SPAN_NEW(int32_t, buf, 3), &key, nad_eq_fn_i32));
    TEST_ASSERT_EQUAL_size_t(0, nad_span_count(NAD_SPAN_NEW(int32_t, buf, 0), &key, nad_eq_fn_i32));
}

/* ========== bounds ========== */

static void test_lower_bound_stops_at_the_first_equal() {
    constexpr int32_t buf[6] = {1, 2, 2, 2, 3, 4};
    constexpr int32_t key = 2;

    TEST_ASSERT_EQUAL_size_t(1, nad_span_lower_bound(NAD_SPAN_NEW(int32_t, buf, 6), &key, nad_cmp_fn_i32));
}

static void test_upper_bound_stops_past_the_last_equal() {
    constexpr int32_t buf[6] = {1, 2, 2, 2, 3, 4};
    constexpr int32_t key = 2;

    TEST_ASSERT_EQUAL_size_t(4, nad_span_upper_bound(NAD_SPAN_NEW(int32_t, buf, 6), &key, nad_cmp_fn_i32));
}

// an absent key gives the insertion point, and both bounds agree on it
static void test_bounds_agree_on_an_absent_key() {
    constexpr int32_t buf[4] = {1, 3, 5, 7};
    constexpr int32_t key = 4;
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 4);

    TEST_ASSERT_EQUAL_size_t(2, nad_span_lower_bound(s, &key, nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_size_t(2, nad_span_upper_bound(s, &key, nad_cmp_fn_i32));
}

// past-the-end is len, which is also the "not found" reading of a bound
static void test_bounds_past_the_end() {
    constexpr int32_t buf[3] = {1, 2, 3};
    constexpr int32_t key = 9;
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 3);

    TEST_ASSERT_EQUAL_size_t(3, nad_span_lower_bound(s, &key, nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_size_t(3, nad_span_upper_bound(s, &key, nad_cmp_fn_i32));
}

static void test_bounds_before_the_start() {
    constexpr int32_t buf[3] = {5, 6, 7};
    constexpr int32_t key = 1;
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 3);

    TEST_ASSERT_EQUAL_size_t(0, nad_span_lower_bound(s, &key, nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_size_t(0, nad_span_upper_bound(s, &key, nad_cmp_fn_i32));
}

static void test_bounds_on_an_empty_span() {
    constexpr int32_t buf[2] = {1, 2};
    constexpr int32_t key = 1;
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 0);

    TEST_ASSERT_EQUAL_size_t(0, nad_span_lower_bound(s, &key, nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_size_t(0, nad_span_upper_bound(s, &key, nad_cmp_fn_i32));
}

// the span between the two bounds is exactly the run of equal elements
static void test_bounds_delimit_the_equal_run() {
    constexpr int32_t buf[7] = {1, 2, 2, 2, 2, 3, 4};
    constexpr int32_t key = 2;
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 7);

    const size_t lo = nad_span_lower_bound(s, &key, nad_cmp_fn_i32);
    const size_t hi = nad_span_upper_bound(s, &key, nad_cmp_fn_i32);

    TEST_ASSERT_EQUAL_size_t(4, hi - lo);
    TEST_ASSERT_EQUAL_size_t(nad_span_count(s, &key, nad_eq_fn_i32), hi - lo);
}

/* ========== bsearch ========== */

static void test_bsearch_finds_a_present_key() {
    constexpr int32_t buf[5] = {1, 3, 5, 7, 9};
    constexpr int32_t key = 7;

    size_t idx = 0;
    TEST_ASSERT_TRUE(nad_span_bsearch(NAD_SPAN_NEW(int32_t, buf, 5), &key, nad_cmp_fn_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(3, idx);
}

// among duplicates it must report the first one, matching lower_bound
static void test_bsearch_reports_the_first_of_the_duplicates() {
    const Tagged buf[5] = {{1, 0}, {2, 10}, {2, 11}, {2, 12}, {3, 0}};
    const Tagged key = {2, -1};

    size_t idx = 0;
    TEST_ASSERT_TRUE(nad_span_bsearch(NAD_SPAN_NEW(Tagged, buf, 5), &key, cmp_tagged, &idx));
    TEST_ASSERT_EQUAL_size_t(1, idx);
    TEST_ASSERT_EQUAL_INT32(10, buf[idx].tag);
}

static void test_bsearch_miss_leaves_the_out_param_alone() {
    constexpr int32_t buf[4] = {1, 3, 5, 7};
    constexpr int32_t key = 4;

    size_t idx = 333;
    TEST_ASSERT_FALSE(nad_span_bsearch(NAD_SPAN_NEW(int32_t, buf, 4), &key, nad_cmp_fn_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(333, idx);
}

// a key past the end drives lower_bound to len — that must not read out of range
static void test_bsearch_key_past_the_end_misses() {
    constexpr int32_t buf[3] = {1, 2, 3};
    constexpr int32_t key = 9;

    size_t idx = 111;
    TEST_ASSERT_FALSE(nad_span_bsearch(NAD_SPAN_NEW(int32_t, buf, 3), &key, nad_cmp_fn_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(111, idx);
}

static void test_bsearch_on_an_empty_span_misses() {
    constexpr int32_t buf[2] = {1, 2};
    constexpr int32_t key = 1;

    size_t idx = 222;
    TEST_ASSERT_FALSE(nad_span_bsearch(NAD_SPAN_NEW(int32_t, buf, 0), &key, nad_cmp_fn_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(222, idx);
}

// every element of a sorted span must be findable, including both edges
static void test_bsearch_finds_every_element() {
    constexpr int32_t buf[6] = {2, 4, 6, 8, 10, 12};
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 6);

    for (size_t i = 0; i < 6; ++i) {
        const int32_t key = buf[i];
        size_t idx = 999;
        TEST_ASSERT_TRUE(nad_span_bsearch(s, &key, nad_cmp_fn_i32, &idx));
        TEST_ASSERT_EQUAL_size_t(i, idx);
    }
}

/* ========== find_if ========== */

static void test_find_if_reports_the_first_match() {
    constexpr int32_t buf[5] = {3, 1, 4, 2, 6};

    size_t idx = 999;
    TEST_ASSERT_TRUE(nad_span_find_if(NAD_SPAN_NEW(int32_t, buf, 5), is_even, nullptr, &idx));
    TEST_ASSERT_EQUAL_size_t(2, idx);
}

static void test_find_if_miss_leaves_the_out_param_alone() {
    constexpr int32_t buf[3] = {1, 3, 5};

    size_t idx = 777;
    TEST_ASSERT_FALSE(nad_span_find_if(NAD_SPAN_NEW(int32_t, buf, 3), is_even, nullptr, &idx));
    TEST_ASSERT_EQUAL_size_t(777, idx);
}

static void test_find_if_on_an_empty_span_finds_nothing() {
    size_t idx = 555;
    TEST_ASSERT_FALSE(nad_span_find_if(NAD_SPAN_NEW(int32_t, nullptr, 0), is_even, nullptr, &idx));
    TEST_ASSERT_EQUAL_size_t(555, idx);
}

// ctx is what makes a predicate parameterizable at all
static void test_find_if_passes_the_ctx_through() {
    constexpr int32_t buf[5] = {1, 2, 3, 4, 5};
    int32_t bound = 3;

    size_t idx = 999;
    TEST_ASSERT_TRUE(nad_span_find_if(NAD_SPAN_NEW(int32_t, buf, 5), greater_than, &bound, &idx));
    TEST_ASSERT_EQUAL_size_t(3, idx);

    bound = 4;
    TEST_ASSERT_TRUE(nad_span_find_if(NAD_SPAN_NEW(int32_t, buf, 5), greater_than, &bound, &idx));
    TEST_ASSERT_EQUAL_size_t(4, idx);
}

static void test_count_if_counts_every_match() {
    constexpr int32_t buf[6] = {1, 2, 3, 4, 5, 6};

    TEST_ASSERT_EQUAL_size_t(3, nad_span_count_if(NAD_SPAN_NEW(int32_t, buf, 6), is_even, nullptr));
}

static void test_count_if_of_none_is_zero() {
    constexpr int32_t buf[3] = {1, 3, 5};

    TEST_ASSERT_EQUAL_size_t(0, nad_span_count_if(NAD_SPAN_NEW(int32_t, buf, 3), is_even, nullptr));
    TEST_ASSERT_EQUAL_size_t(0, nad_span_count_if(NAD_SPAN_NEW(int32_t, nullptr, 0), is_even, nullptr));
}

/* ========== all_of / any_of / none_of ========== */

static void test_all_of_holds_only_when_every_elem_satisfies() {
    constexpr int32_t all[3] = {2, 4, 6};
    constexpr int32_t one_odd[3] = {2, 4, 5};

    TEST_ASSERT_TRUE(nad_span_all_of(NAD_SPAN_NEW(int32_t, all, 3), is_even, nullptr));
    TEST_ASSERT_FALSE(nad_span_all_of(NAD_SPAN_NEW(int32_t, one_odd, 3), is_even, nullptr));
}

static void test_any_of_holds_when_at_least_one_satisfies() {
    constexpr int32_t one_even[3] = {1, 3, 4};
    constexpr int32_t none_even[3] = {1, 3, 5};

    TEST_ASSERT_TRUE(nad_span_any_of(NAD_SPAN_NEW(int32_t, one_even, 3), is_even, nullptr));
    TEST_ASSERT_FALSE(nad_span_any_of(NAD_SPAN_NEW(int32_t, none_even, 3), is_even, nullptr));
}

static void test_none_of_is_the_negation_of_any_of() {
    constexpr int32_t buf[3] = {1, 3, 5};

    TEST_ASSERT_TRUE(nad_span_none_of(NAD_SPAN_NEW(int32_t, buf, 3), is_even, nullptr));
    TEST_ASSERT_FALSE(nad_span_none_of(NAD_SPAN_NEW(int32_t, buf, 3), greater_than, &(int32_t){2}));
}

// the empty span: all_of is vacuously true, any_of false, none_of true
static void test_the_quantifiers_agree_on_an_empty_span() {
    const nad_Span empty = NAD_SPAN_NEW(int32_t, nullptr, 0);

    TEST_ASSERT_TRUE(nad_span_all_of(empty, is_even, nullptr));
    TEST_ASSERT_FALSE(nad_span_any_of(empty, is_even, nullptr));
    TEST_ASSERT_TRUE(nad_span_none_of(empty, is_even, nullptr));
}

/* ========== extremes ========== */

static void test_min_and_max_elem() {
    constexpr int32_t buf[5] = {3, 1, 4, 1, 5};
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 5);

    TEST_ASSERT_EQUAL_size_t(1, nad_span_min_elem(s, nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_size_t(4, nad_span_max_elem(s, nad_cmp_fn_i32));
}

// ties resolve to the earliest index, for both ends
static void test_extremes_pick_the_first_of_equals() {
    const Tagged buf[4] = {{1, 10}, {5, 20}, {1, 11}, {5, 21}};
    const nad_Span s = NAD_SPAN_NEW(Tagged, buf, 4);

    TEST_ASSERT_EQUAL_size_t(0, nad_span_min_elem(s, cmp_tagged));
    TEST_ASSERT_EQUAL_size_t(1, nad_span_max_elem(s, cmp_tagged));
}

static void test_extremes_of_a_single_elem() {
    constexpr int32_t buf[1] = {42};
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 1);

    TEST_ASSERT_EQUAL_size_t(0, nad_span_min_elem(s, nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_size_t(0, nad_span_max_elem(s, nad_cmp_fn_i32));
}

static void test_extremes_at_the_edges() {
    constexpr int32_t ascending[4] = {1, 2, 3, 4};
    constexpr int32_t descending[4] = {4, 3, 2, 1};

    TEST_ASSERT_EQUAL_size_t(0, nad_span_min_elem(NAD_SPAN_NEW(int32_t, ascending, 4), nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_size_t(3, nad_span_max_elem(NAD_SPAN_NEW(int32_t, ascending, 4), nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_size_t(3, nad_span_min_elem(NAD_SPAN_NEW(int32_t, descending, 4), nad_cmp_fn_i32));
    TEST_ASSERT_EQUAL_size_t(0, nad_span_max_elem(NAD_SPAN_NEW(int32_t, descending, 4), nad_cmp_fn_i32));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_find_reports_the_first_match);
    RUN_TEST(test_find_miss_leaves_the_out_param_alone);
    RUN_TEST(test_find_in_an_empty_span_misses);
    RUN_TEST(test_find_matches_the_last_elem);
    RUN_TEST(test_find_index_is_relative_to_the_span);

    RUN_TEST(test_contains);
    RUN_TEST(test_count_tallies_every_match);
    RUN_TEST(test_count_of_an_absent_key_is_zero);

    RUN_TEST(test_lower_bound_stops_at_the_first_equal);
    RUN_TEST(test_upper_bound_stops_past_the_last_equal);
    RUN_TEST(test_bounds_agree_on_an_absent_key);
    RUN_TEST(test_bounds_past_the_end);
    RUN_TEST(test_bounds_before_the_start);
    RUN_TEST(test_bounds_on_an_empty_span);
    RUN_TEST(test_bounds_delimit_the_equal_run);

    RUN_TEST(test_bsearch_finds_a_present_key);
    RUN_TEST(test_bsearch_reports_the_first_of_the_duplicates);
    RUN_TEST(test_bsearch_miss_leaves_the_out_param_alone);
    RUN_TEST(test_bsearch_key_past_the_end_misses);
    RUN_TEST(test_bsearch_on_an_empty_span_misses);
    RUN_TEST(test_bsearch_finds_every_element);

    RUN_TEST(test_min_and_max_elem);
    RUN_TEST(test_extremes_pick_the_first_of_equals);
    RUN_TEST(test_extremes_of_a_single_elem);
    RUN_TEST(test_extremes_at_the_edges);

    RUN_TEST(test_find_if_reports_the_first_match);
    RUN_TEST(test_find_if_miss_leaves_the_out_param_alone);
    RUN_TEST(test_find_if_on_an_empty_span_finds_nothing);
    RUN_TEST(test_find_if_passes_the_ctx_through);
    RUN_TEST(test_count_if_counts_every_match);
    RUN_TEST(test_count_if_of_none_is_zero);

    RUN_TEST(test_all_of_holds_only_when_every_elem_satisfies);
    RUN_TEST(test_any_of_holds_when_at_least_one_satisfies);
    RUN_TEST(test_none_of_is_the_negation_of_any_of);
    RUN_TEST(test_the_quantifiers_agree_on_an_empty_span);


    return UNITY_END();
}
