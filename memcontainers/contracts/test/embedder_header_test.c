#include "memcontainers/contracts/embedder.gen.h"

#include <stdint.h>

_Static_assert(MC_HOST_ABI_MAJOR == 1, "embedder ABI major drift");
_Static_assert(MC_HOST_TICK_WAITING == 0, "kernel waiting mapping drift");
_Static_assert(MC_HOST_TICK_RUNNABLE == 1, "kernel runnable mapping drift");
_Static_assert(MC_HOST_IFACE_VM_V1 != MC_HOST_IFACE_EFFECT_V1, "interface IDs must differ");

int main(void) {
  return MC_HOST_OK == UINT32_C(0) ? 0 : 1;
}
