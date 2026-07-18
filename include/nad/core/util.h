#pragma once

/* ========== macro helpers ========== */

#define NAD_STRINGIFY_(x)   #x
/// expands x, then stringifies.
#define NAD_STRINGIFY(x)    NAD_STRINGIFY_(x)

#define NAD_UNUSED(val)    ((void) (val))