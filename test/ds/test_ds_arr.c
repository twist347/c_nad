#include "nad/ds/arr.h"
#include "nad/alloc/alloc_default.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    nad_Arr *arr;

    assert(NAD_STATUS_IS_OK(NAD_ARR_NEW(int32_t, 10, nad_al_default(), &arr)));

    for (size_t i = 0; i < nad_arr_len(arr); ++i) {
        NAD_ARR_SET(int32_t, arr, i, i * i);
    }

    NAD_ARR_FOREACH(int32_t, it, arr) {
        printf("%d\n", *it);
    }

    nad_arr_drop(arr);
}
