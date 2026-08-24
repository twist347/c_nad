#pragma once

#include "nad/alloc/alloc.h"
#include "nad/core/export.h"

#include <stdio.h>

/// @file

/// @defgroup alloc_log alloc/log
/// @ingroup alloc
/// @brief an allocator that writes down what another one is asked to do
///
/// Every call goes through to the wrapped allocator and is printed on the way, and
/// nothing else happens: put it under a container to see when the container grows.
///
/// The plainest case of the borrowing rule — its own two blocks come from the wrapped
/// allocator and go back to it on drop, so that one has to outlive the log.
///
/// @par Example
/// @snippet alloc/example_log.c wrap
/// @{

/// @name lifetime
/// @{

/// a log over 'wrapped', writing to 'stream'
/// @param wrapped the allocator to pass everything through to, borrowed and not owned
/// @param stream where the lines go; not owned either, and outlives the log too
/// @return the allocator, or null if 'wrapped' could not give it its own two blocks
/// @bigo{1}
[[nodiscard]] NAD_API
nad_Al *nad_al_log_new(nad_Al *wrapped, FILE *stream);

/// gives its own two blocks back to the wrapped allocator
/// @param self the log; null is a no-op
/// @bigo{1}
NAD_API
void nad_al_log_drop(nad_Al *self);

/// @}

/// @}
