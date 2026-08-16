#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"

#include <stdio.h>

/* ========== lifetime ========== */

[[nodiscard]] NAD_API
nad_Al *nad_al_log_new(nad_Al *wrapped, FILE *stream);

NAD_API
void nad_al_log_drop(nad_Al *self);
