#include "mc_host.h"

#include <type_traits>

static_assert(std::is_standard_layout_v<mc_host_api_v1>);
static_assert(sizeof(mc_host_api_v1) == 56);
static_assert(MC_HOST_ABI_MAJOR == 1);

int main() { return 0; }
