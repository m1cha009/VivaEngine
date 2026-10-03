#include "Viva/Assert.h"

// VMA checks its own rules with VMA_ASSERT, including (when the allocator is destroyed) that every
// buffer was freed. Routing it to VIVA_ASSERT reports those failures like ours: logged, then
// stopping in the debugger or exiting. Like VIVA_ASSERT, it only checks in Debug builds.
#define VMA_ASSERT(expr) VIVA_ASSERT(expr)

// VMA is a single header. Everywhere else, including it only declares VMA's functions; here,
// VMA_IMPLEMENTATION makes it also define them, so they're compiled exactly once.
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
