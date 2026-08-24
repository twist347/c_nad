// for @snippet

#include "nad/core/print.h"
#include "nad/core/span.h"

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
    nad_Span nums = NAD_SPAN_OF(int32_t, 5, 3, 1);
    nad_span_print(nums, nad_fprint_i32); // [5, 3, 1]

    // cstr reads a pointer to a pointer, quotes what it finds, and gives a null pointer a
    // form of its own. It needs a type name of its own too: the macro writes 'const T',
    // and 'const const char *' is not a type
    typedef const char *Cstr;

    nad_Span names = NAD_SPAN_OF(Cstr, "ann", nullptr, "bo, jr");
    nad_span_print(names, nad_fprint_cstr); // ["ann", null, "bo, jr"]
    /// [ready]

    /// [use]
    nad_Span people = NAD_SPAN_OF(Person, { "ann", 31 }, { "bo", 4 });
    nad_span_print(people, fprint_person); // [ann(31), bo(4)]
    /// [use]

    return 0;
}
