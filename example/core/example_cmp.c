// for @snippet

#include "terse/algo/search.h"
#include "terse/algo/sort.h"
#include "terse/core/cmp.h"
#include "terse/core/print.h"
#include "terse/core/span.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/// [struct]
// a comparator for a struct delegates to the one for its field, by address — which is
// the only form these come in, and the reason there is no value-taking one
typedef struct {
    const char *name;
    int32_t age;
} Person;

static int cmp_person_by_age(const void *lhs, const void *rhs) {
    const Person *a = lhs;
    const Person *b = rhs;

    return trs_cmp_i32(&a->age, &b->age);
}

static void fprint_person(FILE *stream, const void *x) {
    const Person *p = x;

    fprintf(stream, "%s(%" PRId32 ")", p->name, p->age);
}
/// [struct]

int main() {
    /// [ready]
    const trs_SpanMut nums = TRS_SPAN_OF_MUT(int32_t, 5, 3, 1, 4, 2);

    trs_span_sort(nums, trs_cmp_i32);
    trs_span_mut_print(nums, trs_fprint_i32); // [1, 2, 3, 4, 5]

    trs_span_sort(nums, trs_cmp_desc_i32);
    trs_span_mut_print(nums, trs_fprint_i32); // [5, 4, 3, 2, 1]

    // equality is the other half: it says only whether two are the same, and it is what
    // the hash containers and the searches take
    size_t idx;
    if (trs_span_find(trs_span_mut_to_span(nums), &(int32_t){4}, trs_eq_i32, &idx)) {
        printf("4 is at %zu\n", idx); // 4 is at 1
    }
    /// [ready]

    /// [use]
    const trs_SpanMut people = TRS_SPAN_OF_MUT(Person, { "ann", 31 }, { "bo", 4 }, { "cy", 17 });

    trs_span_sort(people, cmp_person_by_age);
    trs_span_mut_print(people, fprint_person); // [bo(4), cy(17), ann(31)]
    /// [use]

    return 0;
}
