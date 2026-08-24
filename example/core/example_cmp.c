// for @snippet

#include "nad/algo/search.h"
#include "nad/algo/sort.h"
#include "nad/core/cmp.h"
#include "nad/core/print.h"
#include "nad/core/span.h"

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

    return nad_cmp_i32(&a->age, &b->age);
}

static void fprint_person(FILE *stream, const void *x) {
    const Person *p = x;

    fprintf(stream, "%s(%" PRId32 ")", p->name, p->age);
}
/// [struct]

int main() {
    /// [ready]
    nad_SpanMut nums = NAD_SPAN_OF_MUT(int32_t, 5, 3, 1, 4, 2);

    nad_span_sort(nums, nad_cmp_i32);
    nad_span_mut_print(nums, nad_fprint_i32); // [1, 2, 3, 4, 5]

    nad_span_sort(nums, nad_cmp_desc_i32);
    nad_span_mut_print(nums, nad_fprint_i32); // [5, 4, 3, 2, 1]

    // equality is the other half: it says only whether two are the same, and it is what
    // the hash containers and the searches take
    size_t idx;
    if (nad_span_find(nad_span_mut_to_span(nums), &(int32_t){4}, nad_eq_i32, &idx)) {
        printf("4 is at %zu\n", idx); // 4 is at 1
    }
    /// [ready]

    /// [use]
    nad_SpanMut people = NAD_SPAN_OF_MUT(Person, { "ann", 31 }, { "bo", 4 }, { "cy", 17 });

    nad_span_sort(people, cmp_person_by_age);
    nad_span_mut_print(people, fprint_person); // [bo(4), cy(17), ann(31)]
    /// [use]

    return 0;
}
