#include "nad/mem/alloc.h"
#include "nad/mem/alloc_default.h"
#include "nad/mem/alloc_log.h"

int main() {
    nad_Allocator *log_alloc = nad_allocator_log_new(nad_allocator_default(), stdout);

    int *arr = NAD_ALLOC(int, log_alloc, 10);
    if (!arr) {
        return 1;
    }

    int *new_arr = NAD_REALLOC(int, log_alloc, arr, 10, 20);
    if (!new_arr) {
        NAD_DEALLOC(int, log_alloc, arr, 10);
    } else {
        arr = new_arr;
    }

    NAD_DEALLOC(int, log_alloc, arr, 10);

    nad_allocator_log_drop(log_alloc);
}
