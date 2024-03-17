#include <type_traits>
#include <doarr/import.hpp>
#include "../dcc/runtime_struct_defs.h"

static_assert(std::is_standard_layout_v<doarr::imported>);
static_assert(std::is_standard_layout_v<struct guest_fn>);
static_assert(std::is_layout_compatible_v<doarr::imported, struct guest_fn>);
