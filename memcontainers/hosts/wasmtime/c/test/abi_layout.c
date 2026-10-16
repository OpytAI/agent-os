#include "mc_host.h"

#include <stddef.h>

_Static_assert(sizeof(mc_host_header_v1) == 8, "header layout");
_Static_assert(sizeof(mc_host_buffer_v1) == 24, "buffer layout");
_Static_assert(sizeof(mc_host_error_v1) == 48, "error layout");
_Static_assert(sizeof(mc_host_library_info_v1) == 24, "info layout");
_Static_assert(sizeof(mc_host_api_v1) == 56, "API table layout");
_Static_assert(offsetof(mc_host_api_v1, runtime_create) == 24, "table prefix");
_Static_assert(offsetof(mc_host_error_v1, message) == 24, "error message offset");

int main(void) { return 0; }
