#include "mc_host.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *read_file(const char *path, size_t *length) {
  FILE *file = fopen(path, "rb");
  if (!file) return NULL;
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }
  long end = ftell(file);
  if (end <= 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }
  uint8_t *bytes = malloc((size_t)end);
  if (!bytes || fread(bytes, 1, (size_t)end, file) != (size_t)end) {
    free(bytes);
    fclose(file);
    return NULL;
  }
  fclose(file);
  *length = (size_t)end;
  return bytes;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: %s libmc_host.so kernel.wasm image.tar\n", argv[0]);
    return 2;
  }
  size_t kernel_len = 0, image_len = 0;
  uint8_t *kernel = read_file(argv[2], &kernel_len);
  uint8_t *image = read_file(argv[3], &image_len);
  void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!kernel || !image || !library) {
    fprintf(stderr, "cannot open library, kernel, or image\n");
    free(kernel);
    free(image);
    return 1;
  }
  mc_host_get_api_fn get_api = (mc_host_get_api_fn)(uintptr_t)dlsym(library, "mc_host_get_api");
  mc_host_api_v1 api = MC_HOST_INIT(mc_host_api_v1);
  mc_host_runtime_t runtime = 0;
  mc_host_vm_api_v1 vm_api = MC_HOST_INIT(mc_host_vm_api_v1);
  mc_host_handle_t kernel_handle = 0, vm = 0;
  mc_host_kernel_info_v1 kernel_info = MC_HOST_INIT(mc_host_kernel_info_v1);
  mc_host_bytes_v1 layer = {image, image_len};
  mc_host_vm_create_options_v1 create = MC_HOST_INIT(mc_host_vm_create_options_v1);
  create.layers = &layer;
  create.layer_count = 1;
  create.requested_workers = -1;
  int result = 1;
  if (!get_api || get_api(MC_HOST_ABI_MAJOR, 0, sizeof(api), &api) != MC_HOST_OK ||
      api.runtime_create(NULL, &runtime, NULL) != MC_HOST_OK ||
      api.get_interface(runtime, MC_HOST_IFACE_VM_V1, 1, sizeof(vm_api),
                        &vm_api, NULL) != MC_HOST_OK ||
      vm_api.kernel_load(runtime, (mc_host_bytes_v1){kernel, kernel_len},
                         &kernel_handle, &kernel_info, NULL) != MC_HOST_OK ||
      vm_api.vm_create(runtime, kernel_handle, 0, &create, &vm, NULL) != MC_HOST_OK) {
    fprintf(stderr, "cannot create native VM\n");
    goto cleanup;
  }
  for (unsigned attempt = 0; attempt < 4096; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    if (vm_api.vm_drive(runtime, vm, &options, &driven, NULL) != MC_HOST_OK) {
      fprintf(stderr, "VM drive failed\n");
      goto cleanup;
    }
    if (driven.status.flags & MC_HOST_VM_FLAG_AT_PROMPT) {
      puts("AgentOS VM is ready");
      result = 0;
      break;
    }
  }
  if (result) fprintf(stderr, "VM did not reach its prompt\n");

cleanup:
  if (vm) vm_api.vm_close(runtime, vm, NULL);
  if (kernel_handle) vm_api.kernel_close(runtime, kernel_handle, NULL);
  if (runtime) api.runtime_close(runtime, NULL);
  if (library) dlclose(library);
  free(image);
  free(kernel);
  return result;
}
