#include "nad/alloc/alloc.h"
#include "nad/alloc/alloc_default.h"
#include "nad/alloc/alloc_log.h"

int main() {
    nad_Al *log_al = nad_al_log_new(nad_al_default(), stdout);

    int *arr = NAD_ALLOC(int, log_al, 10);
    if (!arr) {
        return 1;
    }

    int *new_arr = NAD_REALLOC(int, log_al, arr, 10, 20);
    if (!new_arr) {
        NAD_DEALLOC(int, log_al, arr, 10);
    } else {
        arr = new_arr;
    }

    NAD_DEALLOC(int, log_al, arr, 20);

    nad_al_log_drop(log_al);
}
