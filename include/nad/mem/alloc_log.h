#pragma once

#include "nad/core/export.h"
#include "nad/mem/alloc.h"

#include <stdio.h>

[[nodiscard]] NAD_API
nad_Allocator *nad_allocator_log_new(nad_Allocator *wrapped, FILE *stream);

NAD_API
void nad_allocator_log_drop(nad_Allocator *self);
