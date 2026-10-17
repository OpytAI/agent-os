#include "mc_host.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void need(int condition, const char *step) {
  if (condition) return;
  fprintf(stderr, "native lifecycle failed: %s\n", step);
  exit(2);
}

static void check(mc_host_result_t result, const char *step) {
  if (result == MC_HOST_OK) return;
  fprintf(stderr, "native lifecycle failed: %s (result %u)\n", step, result);
  exit(2);
}

static uint8_t *read_artifact(const char *path, size_t *length) {
  FILE *file = fopen(path, "rb");
  need(file != NULL, "open artifact");
  need(fseek(file, 0, SEEK_END) == 0, "seek artifact end");
  long length_long = ftell(file);
  need(length_long > 0, "artifact length");
  need(fseek(file, 0, SEEK_SET) == 0, "seek artifact start");
  *length = (size_t)length_long;
  uint8_t *bytes = malloc(*length);
  need(bytes != NULL, "allocate artifact");
  need(fread(bytes, 1, *length, file) == *length, "read artifact");
  need(fclose(file) == 0, "close artifact");
  return bytes;
}

typedef struct wire {
  uint8_t bytes[256];
  size_t length;
} wire;

static void put_u8(wire *out, uint8_t value) {
  need(out->length < sizeof(out->bytes), "wire capacity");
  out->bytes[out->length++] = value;
}

static void put_u16(wire *out, uint16_t value) {
  put_u8(out, (uint8_t)value);
  put_u8(out, (uint8_t)(value >> 8));
}

static void put_u32(wire *out, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) put_u8(out, (uint8_t)(value >> shift));
}

static void put_bytes(wire *out, const uint8_t *bytes, size_t length) {
  need(length <= UINT32_MAX, "wire length");
  put_u32(out, (uint32_t)length);
  for (size_t index = 0; index < length; ++index) put_u8(out, bytes[index]);
}

static uint32_t read_u32(const uint8_t *bytes, size_t length, size_t *offset) {
  need(*offset <= length && length - *offset >= 4, "wire u32");
  uint32_t value = 0;
  for (unsigned shift = 0; shift < 32; shift += 8)
    value |= (uint32_t)bytes[(*offset)++] << shift;
  return value;
}

static uint16_t read_u16(const uint8_t *bytes, size_t length, size_t *offset) {
  need(*offset <= length && length - *offset >= 2, "wire u16");
  uint16_t value = (uint16_t)bytes[*offset] | (uint16_t)bytes[*offset + 1] << 8;
  *offset += 2;
  return value;
}

static uint8_t read_u8(const uint8_t *bytes, size_t length, size_t *offset) {
  need(*offset < length, "wire u8");
  return bytes[(*offset)++];
}

static int contains_bytes(const uint8_t *haystack, size_t length,
                          const char *needle, size_t needle_length) {
  for (size_t index = 0; index + needle_length <= length; ++index)
    if (memcmp(haystack + index, needle, needle_length) == 0) return 1;
  return 0;
}

static wire shell_request(const char *command) {
  wire request = {0}, envelope = {0};
  put_u16(&request, 1);  /* control.kdl ExecRequest v2 */
  put_u8(&request, 2);
  put_u32(&request, 1);  /* EXEC_MODE_SHELL */
  put_u8(&request, 1);
  put_bytes(&request, (const uint8_t *)command, strlen(command));
  put_u32(&request, 0); /* argv */
  put_u8(&request, 0);  /* cwd */
  put_u32(&request, 0); /* env */
  put_u8(&request, 0);  /* stdin */
  put_u16(&envelope, 21); /* embedder.kdl OperationRequest v1 */
  put_u8(&envelope, 1);
  put_u32(&envelope, MC_HOST_OP_EXEC_SHELL);
  put_bytes(&envelope, request.bytes, request.length);
  return envelope;
}

static wire filesystem_request(uint32_t kind, const char *path,
                               const uint8_t *data, size_t data_length) {
  wire request = {0}, envelope = {0};
  put_u16(&request, 27); /* embedder.kdl FilesystemRequest v1 */
  put_u8(&request, 1);
  put_bytes(&request, (const uint8_t *)path, strlen(path));
  put_u8(&request, data != NULL);
  if (data != NULL) put_bytes(&request, data, data_length);
  put_u8(&request, 0); /* target */
  put_u8(&request, 0); /* mode */
  put_u16(&envelope, 21); /* OperationRequest v1 */
  put_u8(&envelope, 1);
  put_u32(&envelope, kind);
  put_bytes(&envelope, request.bytes, request.length);
  return envelope;
}

static void run_filesystem_operations(const mc_host_vm_api_v1 *vm_api,
                                      const mc_host_operation_api_v1 *op_api,
                                      mc_host_runtime_t runtime, mc_host_handle_t vm) {
  const uint8_t content[] = {'c', 0, 'a', 'p', 'i', '\n'};
  wire write_request = filesystem_request(MC_HOST_OP_FS_WRITE,
                                          "/tmp/c-api-data", content, sizeof(content));
  wire read_request = filesystem_request(MC_HOST_OP_FS_READ,
                                         "/tmp/c-api-data", NULL, 0);
  mc_host_handle_t write_op = 0, read_op = 0;
  check(op_api->start(runtime, vm,
                      (mc_host_bytes_v1){write_request.bytes, write_request.length},
                      &write_op, NULL), "start filesystem write");
  check(op_api->start(runtime, vm,
                      (mc_host_bytes_v1){read_request.bytes, read_request.length},
                      &read_op, NULL), "queue filesystem read");
  mc_host_operation_status_v1 write_status = MC_HOST_INIT(mc_host_operation_status_v1);
  mc_host_operation_status_v1 read_status = MC_HOST_INIT(mc_host_operation_status_v1);
  check(op_api->poll(runtime, write_op, &write_status, NULL), "poll filesystem write");
  check(op_api->poll(runtime, read_op, &read_status, NULL), "poll filesystem read");
  need(write_status.state == MC_HOST_OP_NEW && read_status.state == MC_HOST_OP_NEW,
       "filesystem operations wait for drive");
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = 0;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive filesystem operations");
    check(op_api->poll(runtime, write_op, &write_status, NULL), "poll filesystem write");
    check(op_api->poll(runtime, read_op, &read_status, NULL), "poll filesystem read");
    if (write_status.state == MC_HOST_OP_READY && read_status.state == MC_HOST_OP_READY) break;
    need((write_status.state == MC_HOST_OP_NEW || write_status.state == MC_HOST_OP_PENDING ||
          write_status.state == MC_HOST_OP_READY) &&
             (read_status.state == MC_HOST_OP_NEW || read_status.state == MC_HOST_OP_PENDING),
         "filesystem operations progress without failure");
  }
  if (write_status.state != MC_HOST_OP_READY || read_status.state != MC_HOST_OP_READY) {
    fprintf(stderr, "filesystem states: write=%u read=%u\n",
            write_status.state, read_status.state);
    need(0, "filesystem operations complete");
  }
  uint8_t result[256];
  mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, write_op, &buffer, NULL), "read filesystem write result");
  size_t offset = 0;
  need(read_u16(result, buffer.required, &offset) == 22 &&
           read_u8(result, buffer.required, &offset) == 1 &&
           read_u32(result, buffer.required, &offset) == MC_HOST_OP_FS_WRITE &&
           read_u32(result, buffer.required, &offset) == 0 &&
           read_u32(result, buffer.required, &offset) == 0 &&
           offset == buffer.required,
       "filesystem write succeeded");
  buffer.required = 0;
  check(op_api->result(runtime, read_op, &buffer, NULL), "read filesystem read result");
  offset = 0;
  need(read_u16(result, buffer.required, &offset) == 22 &&
           read_u8(result, buffer.required, &offset) == 1 &&
           read_u32(result, buffer.required, &offset) == MC_HOST_OP_FS_READ &&
           read_u32(result, buffer.required, &offset) == 0 &&
           read_u32(result, buffer.required, &offset) == sizeof(content) &&
           buffer.required - offset == sizeof(content) &&
           memcmp(result + offset, content, sizeof(content)) == 0,
       "filesystem read preserves binary data");
  check(op_api->close(runtime, read_op, NULL), "close filesystem read");
  check(op_api->close(runtime, write_op, NULL), "close filesystem write");
}

static void run_control_command(const mc_host_vm_api_v1 *vm_api,
                                const mc_host_operation_api_v1 *op_api,
                                const mc_host_snapshot_api_v1 *snap_api,
                                mc_host_runtime_t runtime, mc_host_handle_t vm) {
  wire envelope = shell_request("git --version");

  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm,
                      (mc_host_bytes_v1){envelope.bytes, envelope.length},
                      &operation, NULL), "start real control command");
  need(operation != 0, "operation handle");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  check(op_api->poll(runtime, operation, &status, NULL), "initial operation poll");
  need(status.state == MC_HOST_OP_PENDING, "poll does not drive the kernel");
  need(vm_api->vm_close(runtime, vm, NULL) == MC_HOST_BUSY,
       "VM cannot close with an operation handle");
  mc_host_snapshot_begin_options_v1 capture = MC_HOST_INIT(mc_host_snapshot_begin_options_v1);
  capture.mode = MC_HOST_SNAPSHOT_FULL;
  mc_host_handle_t rejected_snapshot = 0;
  mc_host_snapshot_info_v1 rejected_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  need(snap_api->begin(runtime, vm, &capture, &rejected_snapshot,
                       &rejected_info, NULL) == MC_HOST_NOT_QUIESCENT &&
           rejected_snapshot == 0,
       "snapshot refuses a live control operation");

  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_OPERATION;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive control command");
    check(op_api->poll(runtime, operation, &status, NULL), "poll control command");
    if (status.state == MC_HOST_OP_READY) break;
    need(status.state == MC_HOST_OP_PENDING, "control command stays pending");
  }
  need(status.state == MC_HOST_OP_READY && status.result_length < 4096,
       "control command completes");
  uint8_t result[4096];
  mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, operation, &buffer, NULL), "read control result");
  need(buffer.required == status.result_length, "control result length");
  size_t offset = 0;
  need(read_u16(result, buffer.required, &offset) == 22 &&
           read_u8(result, buffer.required, &offset) == 1 &&
           read_u32(result, buffer.required, &offset) == MC_HOST_OP_EXEC_SHELL,
       "operation result envelope");
  need(read_u32(result, buffer.required, &offset) == 0, "control exit status");
  uint32_t payload_len = read_u32(result, buffer.required, &offset);
  need(offset + payload_len == buffer.required, "control payload bounds");
  need(read_u16(result, buffer.required, &offset) == 2 &&
           read_u8(result, buffer.required, &offset) == 1,
       "ExecOutcome envelope");
  need(read_u32(result, buffer.required, &offset) == 0, "guest process exit status");
  uint32_t stdout_len = read_u32(result, buffer.required, &offset);
  need(stdout_len <= buffer.required - offset, "stdout bounds");
  need(stdout_len >= sizeof("git") - 1 &&
           contains_bytes(result + offset, stdout_len,
                          "git", sizeof("git") - 1),
       "real guest stdout");
  offset += stdout_len;
  uint32_t stderr_len = read_u32(result, buffer.required, &offset);
  need(stderr_len <= buffer.required - offset && offset + stderr_len == buffer.required,
       "stderr bounds");
  check(op_api->close(runtime, operation, NULL), "close operation");
  need(op_api->poll(runtime, operation, &status, NULL) == MC_HOST_STALE_HANDLE,
       "operation handle retires");
}

static void cancel_vm_with_operation(const mc_host_vm_api_v1 *vm_api,
                                     const mc_host_operation_api_v1 *op_api,
                                     mc_host_runtime_t runtime, mc_host_handle_t vm) {
  wire request = shell_request("git --version");
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm,
                      (mc_host_bytes_v1){request.bytes, request.length},
                      &operation, NULL), "start operation before VM cancellation");
  need(vm_api->vm_cancel_and_close(runtime, vm, NULL) == MC_HOST_BUSY,
       "cancelled VM waits for operation close");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  check(op_api->poll(runtime, operation, &status, NULL), "observe cancelled operation");
  need(status.state == MC_HOST_OP_CANCELLED && status.error == MC_HOST_CANCELLED,
       "operation reports cancellation");
  mc_host_buffer_v1 result = {0};
  need(op_api->result(runtime, operation, &result, NULL) == MC_HOST_CANCELLED,
       "cancelled operation has no result");
  check(op_api->close(runtime, operation, NULL), "close cancelled operation");
  check(vm_api->vm_close(runtime, vm, NULL), "finish closing cancelled VM");
}

static void drain_output(const mc_host_output_api_v1 *output_api,
                         mc_host_runtime_t runtime, mc_host_handle_t vm,
                         int *saw_prompt, const char *marker,
                         size_t *marker_matched, int *marker_count,
                         uint8_t *previous_stdout) {
  for (;;) {
    mc_host_handle_t lease = 0;
    mc_host_output_record_v1 record = MC_HOST_INIT(mc_host_output_record_v1);
    mc_host_result_t result = output_api->next(runtime, vm, &lease, &record, NULL);
    if (result == MC_HOST_WOULD_BLOCK) break;
    check(result, "lease output");
    need(lease != 0 && record.kind == MC_HOST_OUTPUT_DATA,
         "real boot output has no lost records");
    uint8_t *bytes = malloc(record.length ? (size_t)record.length : 1);
    need(bytes != NULL, "allocate output");
    mc_host_buffer_v1 buffer = {bytes, record.length, 0};
    uint64_t copied = 0;
    check(output_api->read(runtime, lease, 0, &buffer, &copied, NULL), "read output");
    need(copied == record.length && buffer.required == record.length,
         "output length");
    if (record.channel == 1) {
      for (size_t index = 0; index < (size_t)copied; ++index) {
        if (*previous_stdout == '$' && bytes[index] == ' ') *saw_prompt = 1;
        *previous_stdout = bytes[index];
        if (marker) {
          *marker_matched = bytes[index] == (uint8_t)marker[*marker_matched]
                                ? *marker_matched + 1 : (bytes[index] == (uint8_t)marker[0] ? 1 : 0);
          if (marker[*marker_matched] == '\0') {
            ++*marker_count;
            *marker_matched = 0;
          }
        }
      }
    }
    check(output_api->ack(runtime, lease, NULL), "ack output");
    free(bytes);
  }
}

static void drive_to_prompt(const mc_host_vm_api_v1 *vm_api,
                            const mc_host_output_api_v1 *output_api,
                            mc_host_runtime_t runtime, mc_host_handle_t vm,
                            const char *marker) {
  int saw_prompt = 0;
  int marker_count = 0;
  size_t marker_matched = 0;
  uint8_t previous_stdout = 0;
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    mc_host_drive_result_v1 result = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &result, NULL), "drive real kernel");
    need(result.ticks <= 128, "bounded drive");
    drain_output(output_api, runtime, vm, &saw_prompt, marker,
                 &marker_matched, &marker_count, &previous_stdout);
    need(result.status.state != MC_HOST_VM_POISONED &&
             result.status.state != MC_HOST_VM_EXITED, "kernel stays alive");
    if ((result.status.flags & MC_HOST_VM_FLAG_AT_PROMPT) &&
        (!marker || marker_count >= 1)) {
      need(saw_prompt, "prompt came from terminal output");
      return;
    }
  }
  need(0, "real kernel reached prompt");
}

static uint8_t *snapshot_bytes(const mc_host_snapshot_api_v1 *snap_api,
                               mc_host_runtime_t runtime, mc_host_handle_t snapshot,
                               uint64_t length) {
  need(length > 128 && length <= SIZE_MAX, "snapshot length");
  uint8_t *bytes = malloc((size_t)length);
  need(bytes != NULL, "allocate snapshot");
  mc_host_buffer_v1 buffer = {bytes, length, 0};
  uint64_t copied = 0;
  check(snap_api->read(runtime, snapshot, 0, &buffer, &copied, NULL), "read snapshot");
  need(copied == length && buffer.required == length, "complete snapshot read");
  return bytes;
}

int main(int argc, char **argv) {
  need(argc == 4, "release library, kernel, and image paths");
  size_t kernel_len = 0, image_len = 0;
  uint8_t *kernel = read_artifact(argv[2], &kernel_len);
  uint8_t *image = read_artifact(argv[3], &image_len);
  void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  need(lib != NULL, "load release library");
  mc_host_get_api_fn get_api = (mc_host_get_api_fn)(uintptr_t)dlsym(lib, "mc_host_get_api");
  need(get_api != NULL, "resolve entry point");
  mc_host_api_v1 api = MC_HOST_INIT(mc_host_api_v1);
  check(get_api(MC_HOST_ABI_MAJOR, 0, sizeof(api), &api), "negotiate API");
  mc_host_runtime_t runtime = 0;
  check(api.runtime_create(NULL, &runtime, NULL), "create runtime");

  mc_host_vm_api_v1 vm_api = MC_HOST_INIT(mc_host_vm_api_v1);
  mc_host_output_api_v1 output_api = MC_HOST_INIT(mc_host_output_api_v1);
  mc_host_snapshot_api_v1 snap_api = MC_HOST_INIT(mc_host_snapshot_api_v1);
  mc_host_operation_api_v1 op_api = MC_HOST_INIT(mc_host_operation_api_v1);
  check(api.get_interface(runtime, MC_HOST_IFACE_VM_V1, 1, sizeof(vm_api), &vm_api, NULL), "VM table");
  check(api.get_interface(runtime, MC_HOST_IFACE_OUTPUT_V1, 1, sizeof(output_api), &output_api, NULL), "output table");
  check(api.get_interface(runtime, MC_HOST_IFACE_SNAPSHOT_V1, 1, sizeof(snap_api), &snap_api, NULL), "snapshot table");
  check(api.get_interface(runtime, MC_HOST_IFACE_OPERATION_V1, 1, sizeof(op_api), &op_api, NULL), "operation table");

  mc_host_handle_t kernel_handle = 0;
  mc_host_kernel_info_v1 kernel_info = MC_HOST_INIT(mc_host_kernel_info_v1);
  uint8_t kernel_prefix[sizeof(mc_host_handle_t)];
  memcpy(kernel_prefix, kernel, sizeof(kernel_prefix));
  need(vm_api.kernel_load(runtime, (mc_host_bytes_v1){kernel, kernel_len},
                          (mc_host_handle_t *)kernel, &kernel_info, NULL) ==
           MC_HOST_INVALID_ARGUMENT &&
           memcmp(kernel_prefix, kernel, sizeof(kernel_prefix)) == 0,
       "kernel load rejects output inside input bytes");
  check(vm_api.kernel_load(runtime, (mc_host_bytes_v1){kernel, kernel_len},
                           &kernel_handle, &kernel_info, NULL), "load real kernel");
  need(kernel_handle != 0, "kernel handle");
  mc_host_bytes_v1 layer = {image, image_len};
  mc_host_vm_create_options_v1 create = MC_HOST_INIT(mc_host_vm_create_options_v1);
  create.layers = &layer;
  create.layer_count = 1;
  create.requested_workers = -1;
  mc_host_handle_t vm = 0;
  check(vm_api.vm_create(runtime, kernel_handle, 0, &create, &vm, NULL), "boot real image");
  need(api.runtime_close(runtime, NULL) == MC_HOST_BUSY, "runtime owns children");
  mc_host_vm_status_v1 status = MC_HOST_INIT(mc_host_vm_status_v1);
  need(vm_api.vm_status(runtime, kernel_handle, &status, NULL) == MC_HOST_WRONG_KIND,
       "kernel is not a VM");
  drive_to_prompt(&vm_api, &output_api, runtime, vm, NULL);
  run_control_command(&vm_api, &op_api, &snap_api, runtime, vm);
  run_filesystem_operations(&vm_api, &op_api, runtime, vm);

  mc_host_snapshot_begin_options_v1 full_options = MC_HOST_INIT(mc_host_snapshot_begin_options_v1);
  full_options.mode = MC_HOST_SNAPSHOT_FULL;
  mc_host_handle_t full_handle = 0;
  mc_host_snapshot_info_v1 full_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api.begin(runtime, vm, &full_options, &full_handle, &full_info, NULL), "capture full snapshot");
  need(full_info.kind == MC_HOST_SNAPSHOT_FULL && full_info.length > 128, "full snapshot metadata");
  need(memcmp(full_info.baseline_id, full_info.content_digest, 32) != 0,
       "baseline identity differs from file digest");
  uint8_t *full = snapshot_bytes(&snap_api, runtime, full_handle, full_info.length);
  mc_host_snapshot_info_v1 inspected = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api.inspect(runtime, (mc_host_bytes_v1){full, full_info.length},
                         (mc_host_bytes_v1){NULL, 0}, &inspected, NULL), "inspect full snapshot");
  need(memcmp(inspected.content_digest, full_info.content_digest, 32) == 0,
       "exact snapshot digest");
  check(snap_api.finish(runtime, full_handle, NULL), "finish full snapshot read");

  mc_host_handle_t baseline = 0;
  mc_host_snapshot_info_v1 baseline_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api.baseline_import(runtime, kernel_handle,
                                 (mc_host_bytes_v1){full, full_info.length},
                                 &baseline, &baseline_info, NULL), "import baseline");
  need(memcmp(baseline_info.baseline_id, full_info.baseline_id, 32) == 0,
       "baseline binding");
  const uint8_t command[] = "git --version\n";
  check(vm_api.terminal_input(runtime, vm,
                              (mc_host_bytes_v1){command, sizeof(command) - 1}, NULL),
        "terminal input");
  drive_to_prompt(&vm_api, &output_api, runtime, vm, "agentos-git");

  mc_host_snapshot_begin_options_v1 delta_options = MC_HOST_INIT(mc_host_snapshot_begin_options_v1);
  delta_options.mode = MC_HOST_SNAPSHOT_INCREMENTAL;
  delta_options.baseline = baseline;
  mc_host_handle_t delta_handle = 0;
  mc_host_snapshot_info_v1 delta_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api.begin(runtime, vm, &delta_options, &delta_handle, &delta_info, NULL),
        "capture incremental snapshot");
  need(delta_info.kind == MC_HOST_SNAPSHOT_INCREMENTAL &&
           memcmp(delta_info.baseline_id, full_info.baseline_id, 32) == 0,
       "incremental baseline identity");
  uint8_t *delta = snapshot_bytes(&snap_api, runtime, delta_handle, delta_info.length);
  check(snap_api.finish(runtime, delta_handle, NULL), "finish incremental snapshot read");

  mc_host_vm_restore_options_v1 restore = MC_HOST_INIT(mc_host_vm_restore_options_v1);
  restore.snapshot = (mc_host_bytes_v1){delta, delta_info.length};
  restore.baseline = (mc_host_bytes_v1){full, full_info.length};
  restore.mode = MC_HOST_RESTORE_DETACHED;
  mc_host_handle_t restored = 0;
  check(vm_api.vm_restore(runtime, kernel_handle, 0, &restore, &restored, NULL),
        "restore real incremental snapshot");
  const uint8_t restored_command[] = "git --version\n";
  check(vm_api.terminal_input(runtime, restored,
                              (mc_host_bytes_v1){restored_command, sizeof(restored_command) - 1}, NULL),
        "restored terminal input");
  drive_to_prompt(&vm_api, &output_api, runtime, restored, "agentos-git");

  cancel_vm_with_operation(&vm_api, &op_api, runtime, restored);
  check(vm_api.vm_close(runtime, vm, NULL), "close source VM");
  check(snap_api.baseline_close(runtime, baseline, NULL), "close baseline");
  check(vm_api.kernel_close(runtime, kernel_handle, NULL), "close kernel");
  check(api.runtime_close(runtime, NULL), "close runtime");
  free(delta);
  free(full);
  free(image);
  free(kernel);
  dlclose(lib);
  return 0;
}
