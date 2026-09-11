#pragma once

#include "tda/alloc/alloc.h"

/// @file

/// @defgroup alloc_default alloc/default
/// @ingroup alloc
/// @brief the allocator that is just malloc
///
/// The default choice, and the usual parent of an arena or a pool.
///
/// @{

/// @name lifetime
/// @{

/// the default allocator
/// @return a static singleton: the same pointer every time, alive for the whole program,
///         and deliberately without a drop
[[nodiscard]] TDA_API
tda_Al *tda_al_default();

/// @}

/// @}
