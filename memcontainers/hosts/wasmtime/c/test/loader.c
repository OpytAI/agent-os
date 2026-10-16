#include "mc_host.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void need(int condition, const char *step) {
  if (condition) return;
  fprintf(stderr, "failed: %s\n", step);
  exit(2);
}

int main(int argc, char **argv) {
  need(argc == 2, "library argument");
  void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  need(lib != NULL, "dlopen");
  mc_host_get_api_fn get_api = (mc_host_get_api_fn)(uintptr_t)dlsym(lib, "mc_host_get_api");
  need(get_api != NULL, "dlsym");

  mc_host_api_v1 api = MC_HOST_INIT(mc_host_api_v1);
  need(get_api(2, 0, sizeof(api), &api) == MC_HOST_UNSUPPORTED_ABI,
       "reject new major");
  need(get_api(1, 1, sizeof(api), &api) == MC_HOST_UNSUPPORTED_ABI,
       "reject new minor");
  need(get_api(1, 0, sizeof(api), &api) == MC_HOST_OK, "get API");
  need(api.header.size == sizeof(api) && api.feature_bits == 0,
       "no unimplemented features advertised");

  mc_host_library_info_v1 info = MC_HOST_INIT(mc_host_library_info_v1);
  need(api.library_info(&info, NULL) == MC_HOST_OK, "library info");
  need(info.abi_major == 1 && info.feature_bits == 0, "library metadata");

  struct {
    mc_host_runtime_options_v1 known;
    uint8_t future[8];
  } options = {MC_HOST_INIT(mc_host_runtime_options_v1), {0}};
  options.known.header.size = sizeof(options);
  options.future[0] = 1;
  mc_host_runtime_t runtime = 99;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_UNSUPPORTED,
       "reject unknown nonzero option");
  need(runtime == 0, "failure clears output");
  options.future[0] = 0;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_OK,
       "accept zero future option");
  need(runtime != 0, "runtime handle");
  need(api.get_interface(runtime, MC_HOST_IFACE_VM_V1, 1, 0, NULL, NULL) ==
           MC_HOST_UNSUPPORTED, "unimplemented VM interface");
  need(api.runtime_close(runtime, NULL) == MC_HOST_OK, "close runtime");
  need(api.runtime_close(runtime, NULL) == MC_HOST_STALE_HANDLE, "stale close");

  mc_host_runtime_t second = 0;
  need(api.runtime_create(NULL, &second, NULL) == MC_HOST_OK,
       "second runtime");
  need(second != runtime, "generation or namespace changes");
  need(api.get_interface(runtime, MC_HOST_IFACE_VM_V1, 1, 0, NULL, NULL) ==
           MC_HOST_STALE_HANDLE, "stale runtime cannot select interface");
  need(api.runtime_close(second, NULL) == MC_HOST_OK, "close second runtime");
  dlclose(lib);
  return 0;
}
