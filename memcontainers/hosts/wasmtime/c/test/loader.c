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
  uint64_t features = MC_HOST_FEATURE_VM_BASIC | MC_HOST_FEATURE_OUTPUT |
                      MC_HOST_FEATURE_SNAPSHOT | MC_HOST_FEATURE_EXEC_OPERATION |
                      MC_HOST_FEATURE_FS_OPERATION;
  need(api.header.size == sizeof(api) && api.feature_bits == features,
       "staged feature set");

  mc_host_library_info_v1 info = MC_HOST_INIT(mc_host_library_info_v1);
  need(api.library_info(&info, NULL) == MC_HOST_OK, "library info");
  need(info.abi_major == 1 && info.feature_bits == features, "library metadata");

  struct {
    mc_host_header_v1 header;
    uint64_t canary;
  } short_info = {{sizeof(mc_host_header_v1), 1, 0}, UINT64_C(0x76543210abcdef01)};
  need(api.library_info((mc_host_library_info_v1 *)&short_info, NULL) == MC_HOST_OK,
       "short output info");
  need(short_info.header.size == sizeof(mc_host_header_v1) &&
           short_info.canary == UINT64_C(0x76543210abcdef01),
       "short info does not overwrite caller memory");

  struct {
    mc_host_library_info_v1 known;
    uint64_t canary;
  } wide_info = {MC_HOST_INIT(mc_host_library_info_v1),
                 UINT64_C(0x12345678abcdef01)};
  wide_info.known.header.size = sizeof(wide_info);
  need(api.library_info(&wide_info.known, NULL) == MC_HOST_OK,
       "wide output info");
  need(wide_info.known.header.size == sizeof(mc_host_library_info_v1) &&
           wide_info.canary == UINT64_C(0x12345678abcdef01),
       "wide info reports written prefix");

  struct {
    mc_host_header_v1 header;
    uint64_t canary;
  } short_error = {{sizeof(mc_host_header_v1), 1, 0}, UINT64_C(0x31415926abcdef01)};
  need(api.runtime_close(0, (mc_host_error_v1 *)&short_error) ==
           MC_HOST_STALE_HANDLE, "short output error");
  need(short_error.header.size == sizeof(mc_host_header_v1) &&
           short_error.canary == UINT64_C(0x31415926abcdef01),
       "short error does not overwrite caller memory");

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

  union {
    mc_host_runtime_options_v1 options;
    mc_host_runtime_t handle;
  } overlap = {.options = MC_HOST_INIT(mc_host_runtime_options_v1)};
  need(api.runtime_create(&overlap.options, &overlap.handle, NULL) ==
           MC_HOST_INVALID_ARGUMENT, "reject overlapping input and output");
  need(overlap.options.header.size == sizeof(mc_host_runtime_options_v1),
       "overlap rejection leaves input unchanged");

  need(api.get_interface(runtime, MC_HOST_IFACE_VM_V1, 1, 0, NULL, NULL) ==
           MC_HOST_INVALID_ARGUMENT, "reject malformed interface request");
  mc_host_header_v1 prefix = {sizeof(prefix), 1, 0};
  need(api.get_interface(runtime, MC_HOST_IFACE_VM_V1, 1, sizeof(prefix),
                         &prefix, NULL) == MC_HOST_OK,
       "VM table prefix");
  need(prefix.size == sizeof(mc_host_vm_api_v1), "VM table reports full size");
  need(api.get_interface(runtime, MC_HOST_IFACE_ATTACHMENT_V1, 1, sizeof(prefix),
                         &prefix, NULL) == MC_HOST_UNSUPPORTED,
       "missing attachment table");
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
