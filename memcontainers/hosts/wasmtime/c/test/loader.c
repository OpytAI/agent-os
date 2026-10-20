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
                      MC_HOST_FEATURE_FS_OPERATION |
                      MC_HOST_FEATURE_AUTOCOMPLETE_OPERATION |
                      MC_HOST_FEATURE_SERVICE_OPERATION |
                      MC_HOST_FEATURE_COMMIT_OPERATION |
                      MC_HOST_FEATURE_ATTACHMENT_PLAN |
                      MC_HOST_FEATURE_HOST_CALL_EFFECT |
                      MC_HOST_FEATURE_MOUNT_OPERATION |
                      MC_HOST_FEATURE_PERSIST_EFFECT |
                      MC_HOST_FEATURE_HTTP_EFFECT |
                      MC_HOST_FEATURE_WS_EFFECT |
                      MC_HOST_FEATURE_NATIVE_CONNECTION |
                      MC_HOST_FEATURE_LIVE_ATTACHMENT;
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
  options.known.max_vms = 17;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject VM limit above hard cap");
  options.known.max_vms = 0;
  options.known.max_drive_ticks = 1001;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject drive limit above hard cap");
  options.known.max_drive_ticks = 0;
  options.known.max_output_bytes = MC_HOST_HARD_MAX_OUTPUT_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject output byte limit above hard cap");
  options.known.max_output_bytes = 3;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_INVALID_ARGUMENT &&
           runtime == 0, "reject output byte limit too small for high water");
  options.known.max_output_bytes = 0;
  options.known.max_output_records = MC_HOST_HARD_MAX_OUTPUT_RECORDS + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject output record limit above hard cap");
  options.known.max_output_records = 3;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_INVALID_ARGUMENT &&
           runtime == 0, "reject output record limit too small for high water");
  options.known.max_output_records = 0;
  options.known.max_snapshot_bytes = MC_HOST_HARD_MAX_SNAPSHOT_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject snapshot limit above hard cap");
  options.known.max_snapshot_bytes = 0;
  options.known.max_snapshot_total_bytes = MC_HOST_HARD_MAX_SNAPSHOT_TOTAL_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject total snapshot quota above hard cap");
  options.known.max_snapshot_total_bytes = 0;
  options.known.max_operations_per_vm = MC_HOST_HARD_MAX_OPERATIONS_PER_VM + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject operation limit above hard cap");
  options.known.max_operations_per_vm = 0;
  options.known.max_effect_events = MC_HOST_HARD_MAX_EFFECT_EVENTS + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject effect slot limit above hard cap");
  options.known.max_effect_events = 0;
  options.known.max_effect_bytes = MC_HOST_HARD_MAX_EFFECT_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject effect queue bytes above hard cap");
  options.known.max_effect_bytes = 3;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_INVALID_ARGUMENT &&
           runtime == 0, "reject effect queue smaller than high water");
  options.known.max_effect_bytes = 0;
  options.known.max_effect_response_bytes = MC_HOST_HARD_MAX_EFFECT_RESPONSE_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject response quota above hard cap");
  options.known.max_effect_response_bytes = 0;
  options.known.max_ws_sockets = MC_HOST_HARD_MAX_WS_SOCKETS + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject socket quota above hard cap");
  options.known.max_ws_sockets = 0;
  options.known.max_ws_incoming_bytes = MC_HOST_HARD_MAX_WS_INCOMING_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject inbound socket bytes above hard cap");
  options.known.max_ws_incoming_bytes = 0;
  options.known.max_kernels = MC_HOST_HARD_MAX_KERNELS + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject kernel handle limit above hard cap");
  options.known.max_kernels = 0;
  options.known.max_kernel_bytes = MC_HOST_HARD_MAX_KERNEL_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject kernel byte limit above hard cap");
  options.known.max_kernel_bytes = 0;
  options.known.max_compiled_modules = MC_HOST_HARD_MAX_COMPILED_MODULES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject module limit above hard cap");
  options.known.max_compiled_modules = 0;
  options.known.max_image_bytes = MC_HOST_HARD_MAX_IMAGE_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject image limit above hard cap");
  options.known.max_image_bytes = 0;
  options.known.max_snapshot_handles = MC_HOST_HARD_MAX_SNAPSHOT_HANDLES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject snapshot handle limit above hard cap");
  options.known.max_snapshot_handles = 0;
  options.known.max_operation_result_bytes = MC_HOST_HARD_MAX_OPERATION_RESULT_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject operation result quota above hard cap");
  options.known.max_operation_result_bytes = 63;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_INVALID_ARGUMENT &&
           runtime == 0, "reject operation result quota below envelope floor");
  options.known.max_operation_result_bytes = 0;
  options.known.max_message_bytes = MC_HOST_HARD_MAX_MESSAGE_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject message quota above hard cap");
  options.known.max_message_bytes = 127;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_INVALID_ARGUMENT &&
           runtime == 0, "reject message quota below envelope floor");
  options.known.max_message_bytes = 0;
  options.known.max_effect_event_bytes = MC_HOST_HARD_MAX_EFFECT_EVENT_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject per-event limit above hard cap");
  options.known.max_effect_event_bytes = 0;
  options.known.max_effect_single_response_bytes =
      MC_HOST_HARD_MAX_EFFECT_SINGLE_RESPONSE_BYTES + 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_RESOURCE_LIMIT &&
           runtime == 0, "reject per-response limit above hard cap");
  options.known.max_effect_single_response_bytes = 0;
  options.known.reserved_message_options = 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_INVALID_ARGUMENT &&
           runtime == 0, "reject nonzero reserved message option");
  options.known.reserved_message_options = 0;
  options.known.header.size = 112;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_OK && runtime != 0,
       "pre-event-quota runtime options remain accepted");
  need(api.runtime_close(runtime, NULL) == MC_HOST_OK,
       "close pre-event-quota-options runtime");
  runtime = 0;
  options.known.header.size = sizeof(mc_host_runtime_options_v1);
  options.known.reserved_result_options = 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_INVALID_ARGUMENT &&
           runtime == 0, "reject nonzero reserved result option");
  options.known.reserved_result_options = 0;
  options.known.header.size = 104;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_OK && runtime != 0,
       "pre-message runtime options remain accepted");
  need(api.runtime_close(runtime, NULL) == MC_HOST_OK,
       "close pre-message-options runtime");
  runtime = 0;
  options.known.header.size = sizeof(mc_host_runtime_options_v1);
  options.known.reserved_resource_options = 1;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_INVALID_ARGUMENT &&
           runtime == 0, "reject nonzero reserved runtime option");
  options.known.reserved_resource_options = 0;
  options.known.header.size = 96;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_OK && runtime != 0,
       "pre-result runtime options remain accepted");
  need(api.runtime_close(runtime, NULL) == MC_HOST_OK,
       "close pre-result-options runtime");
  runtime = 0;
  options.known.header.size = sizeof(mc_host_runtime_options_v1);
  options.known.header.size = 36;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_OK && runtime != 0,
       "old runtime options remain accepted");
  need(api.runtime_close(runtime, NULL) == MC_HOST_OK, "close legacy options runtime");
  runtime = 0;
  options.known.header.size = 56;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_OK && runtime != 0,
       "pre-wake runtime options remain accepted");
  need(api.runtime_close(runtime, NULL) == MC_HOST_OK, "close pre-wake options runtime");
  runtime = 0;
  options.known.header.size = sizeof(options);
  options.known.wake_user_data = &runtime;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_INVALID_ARGUMENT &&
           runtime == 0, "reject wake user data without callback");
  options.known.wake_user_data = NULL;
  options.known.header.size = 72;
  need(api.runtime_create(&options.known, &runtime, NULL) == MC_HOST_OK && runtime != 0,
       "pre-resource runtime options remain accepted");
  need(api.runtime_close(runtime, NULL) == MC_HOST_OK, "close pre-resource options runtime");
  runtime = 0;
  options.known.header.size = sizeof(options);
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
                         &prefix, NULL) == MC_HOST_OK &&
           prefix.size == sizeof(mc_host_attachment_api_v1),
       "attachment table prefix");
  need(api.get_interface(runtime, MC_HOST_IFACE_EFFECT_V1, 1, sizeof(prefix),
                         &prefix, NULL) == MC_HOST_OK &&
           prefix.size == sizeof(mc_host_effect_api_v1),
       "effect table prefix");
  struct {
    uint8_t table[96];
    uint64_t canary;
  } old_effect = {{0}, UINT64_C(0x76543210fedcba98)};
  need(api.get_interface(runtime, MC_HOST_IFACE_EFFECT_V1, 1,
                         sizeof(old_effect.table), old_effect.table, NULL) == MC_HOST_OK,
       "old effect table negotiation");
  need(((mc_host_header_v1 *)old_effect.table)->size == sizeof(mc_host_effect_api_v1) &&
           old_effect.canary == UINT64_C(0x76543210fedcba98),
       "appended effect functions preserve old caller table");
  need(api.runtime_close(runtime, NULL) == MC_HOST_OK, "close runtime");
  need(api.runtime_close(runtime, NULL) == MC_HOST_STALE_HANDLE, "stale close");

  mc_host_header_v1 old_options = {sizeof(old_options), 1, 0};
  mc_host_runtime_t old_runtime = 0;
  need(api.runtime_create((const mc_host_runtime_options_v1 *)&old_options,
                          &old_runtime, NULL) == MC_HOST_OK,
       "old runtime options prefix uses defaults");
  need(api.runtime_close(old_runtime, NULL) == MC_HOST_OK,
       "close old-options runtime");

  struct {
    mc_host_header_v1 header;
    uint32_t max_vms;
    uint32_t max_drive_ticks;
  } legacy_options = {{16, 1, 0}, 1, 16};
  mc_host_runtime_t legacy_runtime = 0;
  need(api.runtime_create((const mc_host_runtime_options_v1 *)&legacy_options,
                          &legacy_runtime, NULL) == MC_HOST_OK,
       "old runtime options retain default output quotas");
  need(api.runtime_close(legacy_runtime, NULL) == MC_HOST_OK,
       "close legacy-options runtime");

  struct {
    mc_host_header_v1 header;
    uint32_t max_vms;
    uint32_t max_drive_ticks;
    uint32_t max_output_bytes;
    uint32_t max_output_records;
  } output_options = {{24, 1, 0}, 1, 16, 4096, 64};
  mc_host_runtime_t output_runtime = 0;
  need(api.runtime_create((const mc_host_runtime_options_v1 *)&output_options,
                          &output_runtime, NULL) == MC_HOST_OK,
       "pre-snapshot runtime options retain default snapshot quota");
  need(api.runtime_close(output_runtime, NULL) == MC_HOST_OK,
       "close pre-snapshot-options runtime");

  struct {
    mc_host_header_v1 header;
    uint32_t max_vms;
    uint32_t max_drive_ticks;
    uint32_t max_output_bytes;
    uint32_t max_output_records;
    uint32_t max_snapshot_bytes;
  } snapshot_options = {{28, 1, 0}, 1, 16, 4096, 64, 8192};
  mc_host_runtime_t snapshot_runtime = 0;
  need(api.runtime_create((const mc_host_runtime_options_v1 *)&snapshot_options,
                          &snapshot_runtime, NULL) == MC_HOST_OK,
       "pre-total runtime options retain default aggregate quota");
  need(api.runtime_close(snapshot_runtime, NULL) == MC_HOST_OK,
       "close pre-total-options runtime");

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
