#pragma once

#include "nad/core/export.h"

#include <stdint.h>

typedef enum : int32_t {
    NAD_STATUS_OK = 0,
    NAD_STATUS_INVALID_ARG,
    NAD_STATUS_OUT_OF_RANGE,
    NAD_STATUS_OUT_OF_MEMORY,
} nad_Status;

[[nodiscard]] NAD_API const char* nad_status_to_str(nad_Status st);

#define NAD_STATUS_IS_OK(st)    ((st) == NAD_STATUS_OK)
#define NAD_STATUS_IS_ERR(st)   ((st) != NAD_STATUS_OK)
