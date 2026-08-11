#include "nad/algo/permute.h"

#include "unity.h"

void setUp() {
}

void tearDown() {
}

// nad/algo/permute.h declares nothing yet and src/algo/permute.c is an empty
// translation unit, so there is no API to exercise. Ignoring is deliberate: an
// empty main() would report a pass and hide the gap.
static void test_permute_is_not_implemented() {
    TEST_IGNORE_MESSAGE("algo/permute has no public API yet");
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_permute_is_not_implemented);

    return UNITY_END();
}
