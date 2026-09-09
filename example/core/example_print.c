// for @snippet

#include "terse/core/print.h"
#include "terse/core/span.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/// [custom]
// a printer is handed the elem by address and writes the value and nothing around it —
// the brackets and the commas belong to whoever prints the container
typedef struct {
    const char *name;
    int32_t age;
} Person;

static void fprint_person(FILE *stream, const void *x) {
    const Person *p = x;

    fprintf(stream, "%s(%" PRId32 ")", p->name, p->age);
}
/// [custom]

int main() {
    /// [ready]
    const trs_Span nums = TRS_SPAN_OF(int32_t, 5, 3, 1);
    trs_span_print(nums, trs_fprint_i32); // [5, 3, 1]

    // cstr reads a pointer to a pointer, quotes what it finds, and gives a null pointer a
    // form of its own. It needs a type name of its own too: the macro writes 'const T',
    // and 'const const char *' is not a type
    typedef const char *Cstr;

    const trs_Span names = TRS_SPAN_OF(Cstr, "ann", nullptr, "bo, jr");
    trs_span_print(names, trs_fprint_cstr); // ["ann", null, "bo, jr"]
    /// [ready]

    /// [use]
    const trs_Span people = TRS_SPAN_OF(Person, { "ann", 31 }, { "bo", 4 });
    trs_span_print(people, fprint_person); // [ann(31), bo(4)]
    /// [use]

    return 0;
}
