#include "nad/nad.h"

#include "support/status.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// the umbrella is one include, and it has to reach every module: a header left out of it
// shows up here as a build error rather than in someone else's project
static void test_the_umbrella_reaches_every_module() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024); // alloc
    TEST_ASSERT_NOT_NULL(arena);

    nad_Vec *v = nullptr;
    NAD_TEST_OK(NAD_VEC_OF(int32_t, arena, &v, 5, 3, 1)); // ds

    nad_span_sort(nad_vec_to_span_mut(v), nad_cmp_i32); // algo, core

    size_t idx;
    TEST_ASSERT_TRUE(nad_span_binary_search(nad_vec_to_span(v), &(int32_t){5}, nad_cmp_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(2, idx);

    TEST_ASSERT_EQUAL_STRING("NAD_STATUS_OK", nad_status_to_str(NAD_STATUS_OK)); // core

    nad_vec_drop(v);
    nad_al_arena_drop(arena);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_the_umbrella_reaches_every_module);

    return UNITY_END();
}
