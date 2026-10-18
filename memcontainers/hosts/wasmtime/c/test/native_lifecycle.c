#include "mc_host.h"

#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

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

static const char *test_tmpdir(void) {
  const char *dir = getenv("TEST_TMPDIR");
  need(dir != NULL && dir[0] == '/', "Bazel test directory");
  return dir;
}

static void test_dir(const char *name, char *out, size_t capacity) {
  int length = snprintf(out, capacity, "%s/%s", test_tmpdir(), name);
  need(length > 0 && (size_t)length < capacity, "host test path");
  need(mkdir(out, 0700) == 0 || errno == EEXIST, "create host test directory");
}

static void *serve_http_once(void *argument) {
  int listener = *(int *)argument;
  int peer = accept(listener, NULL, NULL);
  need(peer >= 0, "accept local HTTP request");
  char request[512];
  ssize_t count = recv(peer, request, sizeof(request), 0);
  need(count >= 11 && memcmp(request, "GET /native", 11) == 0,
       "receive real HTTP request");
  const char response[] = "HTTP/1.0 200 OK\r\nContent-Length: 6\r\n\r\nC-HTTP";
  need(send(peer, response, sizeof(response) - 1, 0) == sizeof(response) - 1,
       "send real HTTP response");
  need(close(peer) == 0 && close(listener) == 0, "close HTTP sockets");
  return NULL;
}

typedef struct wire {
  uint8_t bytes[1024];
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

static void put_u64(wire *out, uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8) put_u8(out, (uint8_t)(value >> shift));
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

static uint64_t read_u64(const uint8_t *bytes, size_t length, size_t *offset) {
  need(*offset <= length && length - *offset >= 8, "wire u64");
  uint64_t value = 0;
  for (unsigned shift = 0; shift < 64; shift += 8)
    value |= (uint64_t)bytes[(*offset)++] << shift;
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
  mc_host_wire_exec_request exec = {
      .mode = 1,
      .has_command = 1,
      .command = {(const uint8_t *)command, strlen(command)}};
  need(mc_host_wire_encode_exec_request(&exec, request.bytes, sizeof(request.bytes),
                                        &request.length) == MC_HOST_WIRE_OK,
       "encode control exec request");
  mc_host_wire_operation_request operation = {
      .kind = MC_HOST_OP_EXEC_SHELL,
      .payload = {request.bytes, request.length}};
  need(mc_host_wire_encode_operation_request(&operation, envelope.bytes,
                                             sizeof(envelope.bytes), &envelope.length) == MC_HOST_WIRE_OK,
       "encode native exec operation");
  return envelope;
}

static wire direct_request(const char *program, const char *argument) {
  wire argv_frames = {0}, request = {0}, envelope = {0};
  mc_host_wire_list argv = {0};
  const char *parts[] = {program, argument};
  for (size_t index = 0; index < 2; ++index) {
    mc_host_wire_exec_arg item = {
        .value = {(const uint8_t *)parts[index], strlen(parts[index])}};
    uint8_t frame[256];
    size_t frame_length = 0;
    need(mc_host_wire_encode_exec_arg(&item, frame, sizeof(frame), &frame_length) ==
             MC_HOST_WIRE_OK,
         "encode direct argv item");
    need(mc_host_wire_list_append(&argv, argv_frames.bytes, sizeof(argv_frames.bytes),
                                 (mc_host_wire_bytes){frame, frame_length}) == MC_HOST_WIRE_OK,
         "append direct argv item");
  }
  mc_host_wire_exec_request exec = {.mode = 2, .argv = argv};
  need(mc_host_wire_encode_exec_request(&exec, request.bytes, sizeof(request.bytes),
                                        &request.length) == MC_HOST_WIRE_OK,
       "encode direct exec request");
  mc_host_wire_operation_request operation = {
      .kind = MC_HOST_OP_EXEC_DIRECT,
      .payload = {request.bytes, request.length}};
  need(mc_host_wire_encode_operation_request(&operation, envelope.bytes,
               sizeof(envelope.bytes), &envelope.length) == MC_HOST_WIRE_OK,
       "encode direct exec operation");
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

static wire autocomplete_request(const char *source) {
  wire request = {0}, envelope = {0};
  mc_host_wire_autocomplete_request completion = {
      .source = {(const uint8_t *)source, strlen(source)},
      .cursor = (uint32_t)strlen(source),
      .limit = 16};
  need(mc_host_wire_encode_autocomplete_request(&completion, request.bytes,
               sizeof(request.bytes), &request.length) == MC_HOST_WIRE_OK,
       "encode autocomplete request");
  mc_host_wire_operation_request operation = {
      .kind = MC_HOST_OP_AUTOCOMPLETE,
      .payload = {request.bytes, request.length}};
  need(mc_host_wire_encode_operation_request(&operation, envelope.bytes,
               sizeof(envelope.bytes), &envelope.length) == MC_HOST_WIRE_OK,
       "encode autocomplete operation");
  return envelope;
}

static wire service_request(const char *service, const char *body) {
  wire request = {0}, envelope = {0};
  mc_host_wire_svc_request call = {
      .service = {(const uint8_t *)service, strlen(service)},
      .request = {(const uint8_t *)body, strlen(body)}};
  need(mc_host_wire_encode_svc_request(&call, request.bytes, sizeof(request.bytes),
                                       &request.length) == MC_HOST_WIRE_OK,
       "encode resident service call");
  mc_host_wire_operation_request operation = {
      .kind = MC_HOST_OP_SERVICE_CALL,
      .payload = {request.bytes, request.length}};
  need(mc_host_wire_encode_operation_request(&operation, envelope.bytes,
               sizeof(envelope.bytes), &envelope.length) == MC_HOST_WIRE_OK,
       "encode service operation");
  return envelope;
}

static wire mount_request(uint32_t kind, const char *path) {
  wire request = {0}, envelope = {0};
  put_u16(&request, 30); /* MountRequest v1 */
  put_u8(&request, 1);
  put_bytes(&request, (const uint8_t *)path, strlen(path));
  put_u8(&request, 0); /* writable mount or unmount */
  put_u16(&envelope, 21);
  put_u8(&envelope, 1);
  put_u32(&envelope, kind);
  put_bytes(&envelope, request.bytes, request.length);
  return envelope;
}

static void run_mount_relay(const mc_host_vm_api_v1 *vm_api,
                            const mc_host_operation_api_v1 *op_api,
                            const mc_host_effect_api_v1 *effect_api,
                            mc_host_runtime_t runtime, mc_host_handle_t vm) {
  wire mount = mount_request(MC_HOST_OP_MOUNT, "/mnt/capi");
  mc_host_handle_t mount_op = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){mount.bytes, mount.length},
                      &mount_op, NULL), "mount registered native driver");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  check(op_api->poll(runtime, mount_op, &status, NULL), "poll native mount");
  need(status.state == MC_HOST_OP_READY, "mount installed before guest sees it");
  check(op_api->close(runtime, mount_op, NULL), "close mount operation");

  wire stat_request = filesystem_request(MC_HOST_OP_FS_STAT, "/mnt/capi/missing", NULL, 0);
  mc_host_handle_t stat_op = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){stat_request.bytes, stat_request.length},
                      &stat_op, NULL), "start mount-backed stat");
  mc_host_handle_t event = 0;
  mc_host_effect_info_v1 info = MC_HOST_INIT(mc_host_effect_info_v1);
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_EFFECT;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive mount-backed stat");
    mc_host_result_t next = effect_api->next(runtime, vm,
                           UINT64_C(1) << MC_HOST_EVENT_HOST_CALL,
                           &event, &info, NULL);
    if (next == MC_HOST_OK) break;
    need(next == MC_HOST_WOULD_BLOCK, "host-call event eventually arrives");
  }
  need(event != 0 && info.kind == MC_HOST_EVENT_HOST_CALL && info.length < 256,
       "lease real mount-driver request");
  uint8_t frame[256];
  mc_host_buffer_v1 event_buffer = {frame, sizeof(frame), 0};
  check(effect_api->read(runtime, event, &event_buffer, NULL), "read mount-driver request");
  size_t offset = 0;
  need(read_u16(frame, event_buffer.required, &offset) == 23 &&
           read_u8(frame, event_buffer.required, &offset) == 1 &&
           read_u32(frame, event_buffer.required, &offset) == MC_HOST_EVENT_HOST_CALL &&
           read_u64(frame, event_buffer.required, &offset) == event &&
           read_u64(frame, event_buffer.required, &offset) == vm &&
           read_u8(frame, event_buffer.required, &offset) == 0 &&
           read_u8(frame, event_buffer.required, &offset) == 0 &&
           read_u32(frame, event_buffer.required, &offset) == 0,
       "effect identity and kind");
  uint32_t payload_len = read_u32(frame, event_buffer.required, &offset);
  need(payload_len == event_buffer.required - offset &&
           read_u16(frame, event_buffer.required, &offset) == 31 &&
           read_u8(frame, event_buffer.required, &offset) == 1,
       "host-call event payload");
  uint32_t name_len = read_u32(frame, event_buffer.required, &offset);
  need(name_len == strlen("/mnt/capi") && name_len <= event_buffer.required - offset &&
           memcmp(frame + offset, "/mnt/capi", name_len) == 0,
       "mount route identity");
  offset += name_len;
  uint32_t request_len = read_u32(frame, event_buffer.required, &offset);
  need(request_len == event_buffer.required - offset && request_len >= 4 &&
           read_u32(frame, event_buffer.required, &offset) == 5,
       "mount driver received one stat request");
  uint32_t path_len = read_u32(frame, event_buffer.required, &offset);
  need(path_len > 0 && path_len < 64 && path_len <= event_buffer.required - offset &&
           !contains_bytes(frame + offset, path_len, "..", 2) &&
           memchr(frame + offset, 0, path_len) == NULL,
       "mount driver path stays inside host test directory");
  char root[512], host_path[640];
  test_dir("mount-root", root, sizeof(root));
  int host_path_len = snprintf(host_path, sizeof(host_path), "%s/%.*s", root,
                               (int)path_len, (const char *)(frame + offset));
  need(host_path_len > 0 && (size_t)host_path_len < sizeof(host_path),
       "host mount path");
  struct stat metadata;
  need(stat(host_path, &metadata) == -1 && errno == ENOENT,
       "host mount path is genuinely absent");

  mc_host_handle_t first_lease = event;
  check(effect_api->release(runtime, first_lease, NULL), "release mount-driver lease");
  check(effect_api->next(runtime, vm, UINT64_C(1) << MC_HOST_EVENT_HOST_CALL,
                         &event, &info, NULL), "lease released mount-driver request");
  need(event != first_lease && info.kind == MC_HOST_EVENT_HOST_CALL,
       "released effect has a fresh lease handle");
  need(effect_api->read(runtime, first_lease, &event_buffer, NULL) == MC_HOST_STALE_HANDLE,
       "released effect handle is stale");

  /* Keep the kernel waiting across the 256-tick stale-inflight threshold. */
  for (int attempt = 0; attempt < 300; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 1;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive pending mount driver");
  }
  mc_host_handle_t duplicate = 0;
  mc_host_effect_info_v1 duplicate_info = MC_HOST_INIT(mc_host_effect_info_v1);
  need(effect_api->next(runtime, vm, 0, &duplicate, &duplicate_info, NULL) == MC_HOST_WOULD_BLOCK,
       "retryable control call did not duplicate the driver request");

  wire reply = {0}, response = {0};
  put_u16(&reply, 32); /* HostCallResponse v1 */
  put_u8(&reply, 1);
  put_u8(&reply, 0); /* not a transport failure */
  uint8_t enoent[4] = {44, 0, 0, 0}; /* ENOENT from constants.kdl */
  put_bytes(&reply, enoent, sizeof(enoent));
  put_u16(&response, 24); /* EffectResponse v1 */
  put_u8(&response, 1);
  put_u32(&response, MC_HOST_EVENT_HOST_CALL);
  put_u64(&response, event);
  put_bytes(&response, reply.bytes, reply.length);
  check(effect_api->respond(runtime, event,
                            (mc_host_bytes_v1){response.bytes, response.length}, NULL),
        "respond to mount-driver request");
  need(effect_api->respond(runtime, event,
                           (mc_host_bytes_v1){response.bytes, response.length}, NULL) ==
           MC_HOST_STALE_HANDLE,
       "host-call response cannot be repeated");
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_OPERATION;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "finish mount-backed stat");
    check(op_api->poll(runtime, stat_op, &status, NULL), "poll mount-backed stat");
    if (status.state == MC_HOST_OP_READY) break;
    need(status.state == MC_HOST_OP_PENDING, "mount-backed stat remains pending");
  }
  need(status.state == MC_HOST_OP_READY && status.result_length < 64,
       "mount-backed stat completes");
  uint8_t result[64];
  mc_host_buffer_v1 result_buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, stat_op, &result_buffer, NULL), "read mount-backed stat result");
  offset = 0;
  need(read_u16(result, result_buffer.required, &offset) == 22 &&
           read_u8(result, result_buffer.required, &offset) == 1 &&
           read_u32(result, result_buffer.required, &offset) == MC_HOST_OP_FS_STAT &&
           (int32_t)read_u32(result, result_buffer.required, &offset) == -44,
       "driver errno reaches the control caller");
  check(op_api->close(runtime, stat_op, NULL), "close mount-backed stat");

  wire unmount = mount_request(MC_HOST_OP_UNMOUNT, "/mnt/capi");
  mc_host_handle_t unmount_op = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){unmount.bytes, unmount.length},
                      &unmount_op, NULL), "unmount native driver");
  check(op_api->close(runtime, unmount_op, NULL), "close unmount operation");
}

static void cancel_mount_relay(const mc_host_vm_api_v1 *vm_api,
                               const mc_host_operation_api_v1 *op_api,
                               const mc_host_effect_api_v1 *effect_api,
                               mc_host_runtime_t runtime, mc_host_handle_t vm) {
  wire mount = mount_request(MC_HOST_OP_MOUNT, "/mnt/capi");
  mc_host_handle_t mount_op = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){mount.bytes, mount.length},
                      &mount_op, NULL), "remount native driver");
  check(op_api->close(runtime, mount_op, NULL), "close remount operation");
  wire stat = filesystem_request(MC_HOST_OP_FS_STAT, "/mnt/capi/cancelled", NULL, 0);
  mc_host_handle_t stat_op = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){stat.bytes, stat.length},
                      &stat_op, NULL), "start cancellable mount stat");
  mc_host_handle_t event = 0;
  mc_host_effect_info_v1 info = MC_HOST_INIT(mc_host_effect_info_v1);
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_EFFECT;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive cancellable mount stat");
    mc_host_result_t next = effect_api->next(runtime, vm, 0, &event, &info, NULL);
    if (next == MC_HOST_OK) break;
    need(next == MC_HOST_WOULD_BLOCK, "cancellable driver request arrives");
  }
  need(event != 0 && info.kind == MC_HOST_EVENT_HOST_CALL,
       "lease cancellable host call");
  mc_host_wire_host_call_response pending_reply = {0};
  wire reply_frame = {0}, response_frame = {0};
  need(mc_host_wire_encode_host_call_response(&pending_reply, reply_frame.bytes,
               sizeof(reply_frame.bytes), &reply_frame.length) == MC_HOST_WIRE_OK,
       "encode pending host-call response");
  mc_host_wire_effect_response pending_response = {
      .kind = MC_HOST_EVENT_HOST_CALL,
      .event_id = (int64_t)event,
      .payload = {reply_frame.bytes, reply_frame.length}};
  need(mc_host_wire_encode_effect_response(&pending_response, response_frame.bytes,
               sizeof(response_frame.bytes), &response_frame.length) == MC_HOST_WIRE_OK,
       "encode pending effect response");
  check(effect_api->response_begin(runtime, event,
          (mc_host_bytes_v1){response_frame.bytes, response_frame.length}, 4, NULL),
        "begin host-call response before VM cancellation");
  const uint8_t partial_errno[] = {44, 0};
  check(effect_api->response_write(runtime, event,
          (mc_host_bytes_v1){partial_errno, sizeof(partial_errno)}, NULL),
        "write partial host-call response");
  need(vm_api->vm_cancel_and_close(runtime, vm, NULL) == MC_HOST_BUSY,
       "cancelled VM waits for adopted effect and operation");
  mc_host_buffer_v1 cancelled_buffer = {0};
  need(effect_api->read(runtime, event, &cancelled_buffer, NULL) == MC_HOST_CANCELLED,
       "original effect becomes cancelled");
  mc_host_handle_t notice = 0;
  mc_host_effect_info_v1 notice_info = MC_HOST_INIT(mc_host_effect_info_v1);
  check(effect_api->next(runtime, vm, 0, &notice, &notice_info, NULL),
        "lease host-call cancellation notice");
  need(notice_info.kind == MC_HOST_EVENT_HOST_CALL_CLOSE && notice != event,
       "one cancellation notice names the adopted effect");
  uint8_t frame[128];
  mc_host_buffer_v1 buffer = {frame, sizeof(frame), 0};
  check(effect_api->read(runtime, notice, &buffer, NULL), "read cancellation notice");
  size_t offset = 0;
  need(read_u16(frame, buffer.required, &offset) == 23 &&
           read_u8(frame, buffer.required, &offset) == 1 &&
           read_u32(frame, buffer.required, &offset) == MC_HOST_EVENT_HOST_CALL_CLOSE,
       "cancellation event envelope");
  offset += 8 + 8; /* event and VM identities */
  need(read_u8(frame, buffer.required, &offset) == 0 &&
           read_u8(frame, buffer.required, &offset) == 0 &&
           read_u32(frame, buffer.required, &offset) == 0 &&
           read_u32(frame, buffer.required, &offset) == 11 &&
           read_u16(frame, buffer.required, &offset) == 33 &&
           read_u8(frame, buffer.required, &offset) == 1 &&
           read_u64(frame, buffer.required, &offset) == event,
       "cancellation notice identifies the original lease");
  check(effect_api->ack(runtime, notice, NULL), "acknowledge effect cancellation");
  need(effect_api->read(runtime, event, &cancelled_buffer, NULL) == MC_HOST_STALE_HANDLE,
       "ack retires original effect handle");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  check(op_api->poll(runtime, stat_op, &status, NULL), "observe cancelled filesystem call");
  need(status.state == MC_HOST_OP_CANCELLED, "filesystem operation cancelled");
  check(op_api->close(runtime, stat_op, NULL), "close cancelled filesystem call");
  check(vm_api->vm_close(runtime, vm, NULL), "finish closing source VM");
}

static void run_persist_relay(const mc_host_vm_api_v1 *vm_api,
                              const mc_host_operation_api_v1 *op_api,
                              const mc_host_effect_api_v1 *effect_api,
                              mc_host_runtime_t runtime, mc_host_handle_t vm) {
  wire read = filesystem_request(MC_HOST_OP_FS_READ, "/var/persist/capi-missing", NULL, 0);
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){read.bytes, read.length},
                      &operation, NULL), "start persistence-backed read");
  mc_host_handle_t event = 0;
  mc_host_effect_info_v1 info = MC_HOST_INIT(mc_host_effect_info_v1);
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_EFFECT;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive persistence-backed read");
    mc_host_result_t next = effect_api->next(runtime, vm, 0, &event, &info, NULL);
    if (next == MC_HOST_OK) break;
    need(next == MC_HOST_WOULD_BLOCK, "persistence event eventually arrives");
  }
  need(event != 0 && info.kind == MC_HOST_EVENT_PERSIST_GET && info.length < 256,
       "lease real persistence request");
  uint8_t frame[256];
  mc_host_buffer_v1 buffer = {frame, sizeof(frame), 0};
  check(effect_api->read(runtime, event, &buffer, NULL), "read persistence request");
  size_t offset = 0;
  need(read_u16(frame, buffer.required, &offset) == 23 &&
           read_u8(frame, buffer.required, &offset) == 1 &&
           read_u32(frame, buffer.required, &offset) == MC_HOST_EVENT_PERSIST_GET &&
           read_u64(frame, buffer.required, &offset) == event &&
           read_u64(frame, buffer.required, &offset) == vm,
       "persistence event identity");
  offset += 2; /* absent operation and socket identities */
  need(read_u32(frame, buffer.required, &offset) == 0,
       "persistence event flags");
  uint32_t payload_len = read_u32(frame, buffer.required, &offset);
  need(payload_len == buffer.required - offset &&
           read_u16(frame, buffer.required, &offset) == 35 &&
           read_u8(frame, buffer.required, &offset) == 1,
       "persistence request payload");
  uint32_t key_len = read_u32(frame, buffer.required, &offset);
  need(key_len == strlen("capi-missing") && key_len <= buffer.required - offset &&
           memcmp(frame + offset, "capi-missing", key_len) == 0,
       "guest persistence key");
  char persist_root[512], persist_path[640];
  test_dir("persist-root", persist_root, sizeof(persist_root));
  int persist_path_len = snprintf(persist_path, sizeof(persist_path), "%s/%.*s",
                                  persist_root, (int)key_len, (const char *)(frame + offset));
  need(persist_path_len > 0 && (size_t)persist_path_len < sizeof(persist_path),
       "host persistence path");
  FILE *stored = fopen(persist_path, "rb");
  need(stored == NULL && errno == ENOENT, "host persistence key is genuinely absent");
  offset += key_len;
  need(read_u8(frame, buffer.required, &offset) == 0 && offset == buffer.required,
       "GET carries no value");

  wire payload = {0}, response = {0};
  put_u16(&payload, 36); /* PersistEffectResponse v1 */
  put_u8(&payload, 1);
  put_u8(&payload, 0); /* no transport failure */
  const uint8_t absent = 0;
  put_bytes(&payload, &absent, 1); /* PERSIST_GET_ABSENT */
  put_u16(&response, 24);
  put_u8(&response, 1);
  put_u32(&response, MC_HOST_EVENT_PERSIST_GET);
  put_u64(&response, event);
  put_bytes(&response, payload.bytes, payload.length);
  check(effect_api->respond(runtime, event,
                            (mc_host_bytes_v1){response.bytes, response.length}, NULL),
        "answer persistence GET");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  int list_requests = 0;
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_OPERATION | MC_HOST_DRIVE_ON_EFFECT;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "finish persistence read");
    check(op_api->poll(runtime, operation, &status, NULL), "poll persistence read");
    if (status.state == MC_HOST_OP_READY) break;
    need(status.state == MC_HOST_OP_PENDING, "persistence read stays pending");
    mc_host_handle_t next_event = 0;
    mc_host_effect_info_v1 next_info = MC_HOST_INIT(mc_host_effect_info_v1);
    mc_host_result_t next = effect_api->next(runtime, vm, 0, &next_event, &next_info, NULL);
    if (next == MC_HOST_WOULD_BLOCK) continue;
    check(next, "lease follow-up persistence request");
    need(next_info.kind == MC_HOST_EVENT_PERSIST_LIST && list_requests++ == 0,
         "missing key checks directory prefix once");
    DIR *directory = opendir(persist_root);
    need(directory != NULL, "open host persistence directory");
    struct dirent *entry;
    int listed = 0;
    while ((entry = readdir(directory)) != NULL) {
      if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        ++listed;
    }
    need(closedir(directory) == 0 && listed == 0,
         "host persistence directory has no matching keys");
    wire list_payload = {0}, list_response = {0};
    put_u16(&list_payload, 36);
    put_u8(&list_payload, 1);
    put_u8(&list_payload, 0);
    put_u32(&list_payload, 0); /* empty directory listing */
    put_u16(&list_response, 24);
    put_u8(&list_response, 1);
    put_u32(&list_response, MC_HOST_EVENT_PERSIST_LIST);
    put_u64(&list_response, next_event);
    put_bytes(&list_response, list_payload.bytes, list_payload.length);
    check(effect_api->respond(runtime, next_event,
                              (mc_host_bytes_v1){list_response.bytes, list_response.length}, NULL),
          "answer persistence LIST");
  }
  need(status.state == MC_HOST_OP_READY && status.result_length < 64 && list_requests == 1,
       "persistence-backed read completes");
  uint8_t result[64];
  mc_host_buffer_v1 result_buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, operation, &result_buffer, NULL), "read persistence result");
  offset = 0;
  need(read_u16(result, result_buffer.required, &offset) == 22 &&
           read_u8(result, result_buffer.required, &offset) == 1 &&
           read_u32(result, result_buffer.required, &offset) == MC_HOST_OP_FS_READ &&
           (int32_t)read_u32(result, result_buffer.required, &offset) == -44,
       "absent persistence key becomes guest ENOENT");
  check(op_api->close(runtime, operation, NULL), "close persistence read");
}

static void run_http_relay(const mc_host_vm_api_v1 *vm_api,
                           const mc_host_operation_api_v1 *op_api,
                           const mc_host_effect_api_v1 *effect_api,
                           mc_host_runtime_t runtime, mc_host_handle_t vm) {
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  need(listener >= 0, "create HTTP listener");
  struct sockaddr_in address = {0};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  need(bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0,
       "bind loopback HTTP listener");
  need(listen(listener, 1) == 0, "listen for HTTP request");
  socklen_t address_length = sizeof(address);
  need(getsockname(listener, (struct sockaddr *)&address, &address_length) == 0,
       "read loopback port");
  pthread_t server;
  need(pthread_create(&server, NULL, serve_http_once, &listener) == 0,
       "start real HTTP server");

  char script[512];
  int script_length = snprintf(script, sizeof(script),
      "local sys=require('sys'); local r=assert(sys.net.fetch('http://127.0.0.1:%u/native')); "
      "assert(r.status==200,tostring(r.status)); "
      "local denied=sys.net.fetch('http://127.0.0.1:%u/native',{headers={['X-MC-Connection']='test.owner.secret'}}); "
      "assert(denied==nil); print('native-http-ok')",
      (unsigned)ntohs(address.sin_port),
      (unsigned)ntohs(address.sin_port));
  need(script_length > 0 && (size_t)script_length < sizeof(script), "guest HTTP script");
  wire write = filesystem_request(MC_HOST_OP_FS_WRITE, "/tmp/native-http.luau",
                                  (const uint8_t *)script, (size_t)script_length);
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){write.bytes, write.length},
                      &operation, NULL), "write guest HTTP script");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 2048; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive script write");
    check(op_api->poll(runtime, operation, &status, NULL), "poll script write");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(status.state == MC_HOST_OP_READY, "guest HTTP script written");
  check(op_api->close(runtime, operation, NULL), "close script write");

  wire exec = shell_request("luau /tmp/native-http.luau");
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){exec.bytes, exec.length},
                      &operation, NULL), "start guest HTTP fetch");
  int handled = 0;
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive guest HTTP fetch");
    mc_host_handle_t event = 0;
    mc_host_effect_info_v1 info = MC_HOST_INIT(mc_host_effect_info_v1);
    mc_host_result_t next = effect_api->next(runtime, vm,
        UINT64_C(1) << MC_HOST_EVENT_HTTP_REQUEST, &event, &info, NULL);
    if (next == MC_HOST_OK) {
      need(!handled && info.kind == MC_HOST_EVENT_HTTP_REQUEST, "one guest HTTP request");
      uint8_t frame[2048];
      mc_host_buffer_v1 buffer = {frame, sizeof(frame), 0};
      check(effect_api->read(runtime, event, &buffer, NULL), "read HTTP event");
      mc_host_wire_effect_event decoded;
      need(mc_host_wire_decode_effect_event(frame, buffer.required, &decoded) == MC_HOST_WIRE_OK &&
           decoded.kind == MC_HOST_EVENT_HTTP_REQUEST && decoded.event_id == (int64_t)event &&
           !decoded.has_operation_id && !decoded.has_socket_id,
           "HTTP event envelope");
      mc_host_wire_http_effect_request http_request;
      need(mc_host_wire_decode_http_effect_request(decoded.payload.data,
                                                    decoded.payload.len, &http_request) == MC_HOST_WIRE_OK &&
           contains_bytes(http_request.request.data, http_request.request.len,
                          "GET http://127.0.0.1:", 21) &&
           !contains_bytes(http_request.request.data, http_request.request.len,
                           "X-MC-Connection", 15),
           "raw unmarked guest HTTP request");

      int client = socket(AF_INET, SOCK_STREAM, 0);
      need(client >= 0 && connect(client, (struct sockaddr *)&address,
                                  sizeof(address)) == 0, "connect application transport");
      const char request[] = "GET /native HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n";
      need(send(client, request, sizeof(request) - 1, 0) == sizeof(request) - 1,
           "send application HTTP request");
      uint8_t response[512];
      size_t response_length = 0;
      for (;;) {
        ssize_t received = recv(client, response + response_length,
                                sizeof(response) - response_length, 0);
        need(received >= 0, "read real HTTP response");
        if (received == 0) break;
        response_length += (size_t)received;
        need(response_length < sizeof(response), "bounded local HTTP response");
      }
      need(close(client) == 0, "close application HTTP socket");
      size_t separator = 0;
      while (separator + 4 <= response_length &&
             memcmp(response + separator, "\r\n\r\n", 4) != 0) ++separator;
      need(separator + 4 <= response_length &&
           contains_bytes(response, separator, "200 OK", 6),
           "HTTP response head");
      size_t body_length = response_length - separator - 4;
      need(body_length == 6 && memcmp(response + separator + 4, "C-HTTP", 6) == 0,
           "real HTTP response body");
      wire payload = {0}, answer = {0};
      const char head[] = "200 OK\r\nContent-Length: 6\r\n\r\n";
      mc_host_wire_http_effect_response http_response = {
          .failed = 0,
          .head = {(const uint8_t *)head, sizeof(head) - 1},
          .body = {NULL, 0}};
      need(mc_host_wire_encode_http_effect_response(&http_response, payload.bytes,
                  sizeof(payload.bytes), &payload.length) == MC_HOST_WIRE_OK,
           "encode HTTP response");
      mc_host_wire_effect_response effect_response = {
          .kind = MC_HOST_EVENT_HTTP_REQUEST,
          .event_id = (int64_t)event,
          .payload = {payload.bytes, payload.length}};
      need(mc_host_wire_encode_effect_response(&effect_response, answer.bytes,
                  sizeof(answer.bytes), &answer.length) == MC_HOST_WIRE_OK,
           "encode native effect response");
      check(effect_api->response_begin(runtime, event,
          (mc_host_bytes_v1){answer.bytes, answer.length}, body_length, NULL),
          "begin streamed HTTP response");
      check(effect_api->response_write(runtime, event,
          (mc_host_bytes_v1){response + separator + 4, 2}, NULL),
          "write first HTTP response chunk");
      check(effect_api->response_abort(runtime, event, NULL),
            "abort incomplete HTTP response");
      check(effect_api->response_begin(runtime, event,
          (mc_host_bytes_v1){answer.bytes, answer.length}, body_length, NULL),
          "retry streamed HTTP response");
      check(effect_api->response_write(runtime, event,
          (mc_host_bytes_v1){response + separator + 4, 2}, NULL),
          "write retried HTTP response chunk");
      need(effect_api->response_finish(runtime, event, NULL) == MC_HOST_INVALID_STATE,
           "incomplete response cannot become visible to guest");
      mc_host_drive_options_v1 pending_options = MC_HOST_INIT(mc_host_drive_options_v1);
      pending_options.max_ticks = 16;
      mc_host_drive_result_v1 pending_drive = MC_HOST_INIT(mc_host_drive_result_v1);
      check(vm_api->vm_drive(runtime, vm, &pending_options, &pending_drive, NULL),
            "drive while HTTP response is incomplete");
      check(op_api->poll(runtime, operation, &status, NULL),
            "poll guest waiting for complete HTTP response");
      need(status.state == MC_HOST_OP_PENDING,
           "partial application response stays hidden from guest");
      check(effect_api->response_write(runtime, event,
          (mc_host_bytes_v1){response + separator + 6, body_length - 2}, NULL),
          "write final HTTP response chunk");
      check(effect_api->response_finish(runtime, event, NULL),
            "finish streamed HTTP response");
      handled = 1;
    } else {
      need(next == MC_HOST_WOULD_BLOCK, "HTTP event queue");
    }
    check(op_api->poll(runtime, operation, &status, NULL), "poll guest HTTP fetch");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(handled && status.state == MC_HOST_OP_READY && status.result_length < 4096,
       "guest HTTP fetch completes");
  uint8_t result[4096];
  mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, operation, &buffer, NULL), "read guest HTTP result");
  need(contains_bytes(result, buffer.required, "native-http-ok", 14),
       "real guest saw HTTP status");
  check(op_api->close(runtime, operation, NULL), "close guest HTTP fetch");
  need(pthread_join(server, NULL) == 0, "join real HTTP server");
}

static void run_layer_commit(const mc_host_operation_api_v1 *op_api,
                             mc_host_runtime_t runtime, mc_host_handle_t vm) {
  wire request = {0};
  put_u16(&request, 21); /* OperationRequest v1 */
  put_u8(&request, 1);
  put_u32(&request, MC_HOST_OP_COMMIT_LAYER);
  put_u32(&request, 0); /* empty payload */
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm,
                      (mc_host_bytes_v1){request.bytes, request.length},
                      &operation, NULL), "commit real guest overlay");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  check(op_api->poll(runtime, operation, &status, NULL), "poll committed layer");
  need(status.state == MC_HOST_OP_READY && status.result_length > 100 &&
           status.result_length <= 16 * 1024 * 1024,
       "layer commit returned a bounded result");
  uint8_t *result = malloc((size_t)status.result_length);
  need(result != NULL, "allocate layer result");
  mc_host_buffer_v1 buffer = {result, status.result_length, 0};
  check(op_api->result(runtime, operation, &buffer, NULL), "read committed layer");
  size_t offset = 0;
  need(read_u16(result, buffer.required, &offset) == 22 &&
           read_u8(result, buffer.required, &offset) == 1 &&
           read_u32(result, buffer.required, &offset) == MC_HOST_OP_COMMIT_LAYER &&
           read_u32(result, buffer.required, &offset) == 0,
       "commit operation envelope");
  uint32_t payload_len = read_u32(result, buffer.required, &offset);
  need(payload_len == buffer.required - offset &&
           read_u16(result, buffer.required, &offset) == 28 &&
           read_u8(result, buffer.required, &offset) == 1,
       "layer commit result envelope");
  uint32_t digest_len = read_u32(result, buffer.required, &offset);
  need(digest_len == 71 && digest_len <= buffer.required - offset &&
           memcmp(result + offset, "sha256:", 7) == 0,
       "content address of committed layer");
  offset += digest_len;
  uint32_t layer_len = read_u32(result, buffer.required, &offset);
  need(layer_len == buffer.required - offset && layer_len > 512 &&
           contains_bytes(result + offset, layer_len, "c-api-data", 10),
       "committed layer contains guest filesystem write");
  free(result);
  check(op_api->close(runtime, operation, NULL), "close layer commit");
}

static void run_service_call(const mc_host_vm_api_v1 *vm_api,
                             const mc_host_operation_api_v1 *op_api,
                             mc_host_runtime_t runtime, mc_host_handle_t vm) {
  wire request = service_request("tools", "{\"op\":\"list\"}");
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm,
                      (mc_host_bytes_v1){request.bytes, request.length},
                      &operation, NULL), "start tools service call");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  check(op_api->poll(runtime, operation, &status, NULL), "poll tools call before drive");
  need(status.state == MC_HOST_OP_PENDING, "service call does not drive during start");
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_OPERATION;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive tools service call");
    check(op_api->poll(runtime, operation, &status, NULL), "poll tools service call");
    if (status.state == MC_HOST_OP_READY) break;
    need(status.state == MC_HOST_OP_PENDING, "service call stays pending");
  }
  need(status.state == MC_HOST_OP_READY && status.result_length < 4096,
       "tools service call completes");
  uint8_t result[4096];
  mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, operation, &buffer, NULL), "read tools service result");
  mc_host_wire_operation_result operation_result;
  need(mc_host_wire_decode_operation_result(result, buffer.required, &operation_result) ==
           MC_HOST_WIRE_OK && operation_result.kind == MC_HOST_OP_SERVICE_CALL &&
           operation_result.status == 0,
       "service operation result envelope");
  mc_host_wire_svc_response service_response;
  need(mc_host_wire_decode_svc_response(operation_result.payload.data,
                                       operation_result.payload.len, &service_response) ==
           MC_HOST_WIRE_OK && service_response.status == 0 &&
           contains_bytes(service_response.body.data, service_response.body.len, "\"tools\"", 7),
       "real tools broker responded");
  check(op_api->close(runtime, operation, NULL), "close tools service call");
}

static void run_autocomplete(const mc_host_vm_api_v1 *vm_api,
                             const mc_host_operation_api_v1 *op_api,
                             mc_host_runtime_t runtime, mc_host_handle_t vm) {
  wire request = autocomplete_request("gi");
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm,
                      (mc_host_bytes_v1){request.bytes, request.length},
                      &operation, NULL), "start autocomplete");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  check(op_api->poll(runtime, operation, &status, NULL), "poll autocomplete before drive");
  need(status.state == MC_HOST_OP_NEW, "autocomplete does not drive during start");
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_OPERATION;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive autocomplete");
    check(op_api->poll(runtime, operation, &status, NULL), "poll autocomplete");
    if (status.state == MC_HOST_OP_READY) break;
    need(status.state == MC_HOST_OP_PENDING, "autocomplete stays pending");
  }
  need(status.state == MC_HOST_OP_READY && status.result_length < 4096,
       "autocomplete completes");
  uint8_t result[4096];
  mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, operation, &buffer, NULL), "read autocomplete result");
  mc_host_wire_operation_result operation_result;
  need(mc_host_wire_decode_operation_result(result, buffer.required, &operation_result) ==
           MC_HOST_WIRE_OK && operation_result.kind == MC_HOST_OP_AUTOCOMPLETE &&
           operation_result.status == 0,
       "autocomplete operation envelope");
  mc_host_wire_autocomplete_result completion;
  need(mc_host_wire_decode_autocomplete_result(operation_result.payload.data,
                 operation_result.payload.len, &completion) == MC_HOST_WIRE_OK &&
           completion.replace_start == 0 && completion.replace_end == 2,
       "real shell autocomplete result");
  check(op_api->close(runtime, operation, NULL), "close autocomplete");
}

static void run_filesystem_operations(const mc_host_vm_api_v1 *vm_api,
                                      const mc_host_operation_api_v1 *op_api,
                                      mc_host_runtime_t runtime, mc_host_handle_t vm) {
  const uint8_t content[] = {'c', 0, 'a', 'p', 'i', '\n'};
  wire write_request = filesystem_request(MC_HOST_OP_FS_WRITE,
                                          "/etc/c-api-data", content, sizeof(content));
  wire read_request = filesystem_request(MC_HOST_OP_FS_READ,
                                         "/etc/c-api-data", NULL, 0);
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
  mc_host_wire_operation_result operation_result;
  need(mc_host_wire_decode_operation_result(result, buffer.required, &operation_result) ==
           MC_HOST_WIRE_OK && operation_result.kind == MC_HOST_OP_EXEC_SHELL,
       "operation result envelope");
  need(operation_result.status == 0, "control exit status");
  mc_host_wire_exec_outcome outcome;
  need(mc_host_wire_decode_exec_outcome(operation_result.payload.data,
             operation_result.payload.len, &outcome) == MC_HOST_WIRE_OK &&
           outcome.exit_code == 0,
       "guest process exit status");
  need(outcome.stdout.len >= sizeof("git") - 1 &&
           contains_bytes(outcome.stdout.data, outcome.stdout.len,
                          "git", sizeof("git") - 1),
       "real guest stdout");
  check(op_api->close(runtime, operation, NULL), "close operation");
  need(op_api->poll(runtime, operation, &status, NULL) == MC_HOST_STALE_HANDLE,
       "operation handle retires");

  wire direct = direct_request("/bin/git", "--version");
  check(op_api->start(runtime, vm,
                      (mc_host_bytes_v1){direct.bytes, direct.length},
                      &operation, NULL), "start direct argv command");
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_OPERATION;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive direct argv command");
    check(op_api->poll(runtime, operation, &status, NULL), "poll direct argv command");
    if (status.state == MC_HOST_OP_READY) break;
    need(status.state == MC_HOST_OP_PENDING, "direct argv command stays pending");
  }
  need(status.state == MC_HOST_OP_READY && status.result_length < sizeof(result),
       "direct argv command completes");
  buffer.required = 0;
  check(op_api->result(runtime, operation, &buffer, NULL), "read direct argv result");
  need(mc_host_wire_decode_operation_result(result, buffer.required, &operation_result) ==
           MC_HOST_WIRE_OK && operation_result.kind == MC_HOST_OP_EXEC_DIRECT &&
           operation_result.status == 0 &&
           mc_host_wire_decode_exec_outcome(operation_result.payload.data,
                      operation_result.payload.len, &outcome) == MC_HOST_WIRE_OK &&
           outcome.exit_code == 0 &&
           contains_bytes(outcome.stdout.data, outcome.stdout.len, "git", 3),
       "real direct argv execution");
  check(op_api->close(runtime, operation, NULL), "close direct argv operation");
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
  mc_host_attachment_api_v1 attachment_api = MC_HOST_INIT(mc_host_attachment_api_v1);
  mc_host_effect_api_v1 effect_api = MC_HOST_INIT(mc_host_effect_api_v1);
  check(api.get_interface(runtime, MC_HOST_IFACE_VM_V1, 1, sizeof(vm_api), &vm_api, NULL), "VM table");
  check(api.get_interface(runtime, MC_HOST_IFACE_OUTPUT_V1, 1, sizeof(output_api), &output_api, NULL), "output table");
  check(api.get_interface(runtime, MC_HOST_IFACE_SNAPSHOT_V1, 1, sizeof(snap_api), &snap_api, NULL), "snapshot table");
  check(api.get_interface(runtime, MC_HOST_IFACE_OPERATION_V1, 1, sizeof(op_api), &op_api, NULL), "operation table");
  check(api.get_interface(runtime, MC_HOST_IFACE_ATTACHMENT_V1, 1, sizeof(attachment_api),
                          &attachment_api, NULL), "attachment table");
  check(api.get_interface(runtime, MC_HOST_IFACE_EFFECT_V1, 1, sizeof(effect_api),
                          &effect_api, NULL), "effect table");

  mc_host_handle_t plan = 0;
  check(attachment_api.plan_create(runtime, &plan, NULL), "create attachment plan");
  wire route = {0}, definition = {0};
  put_u16(&route, 29); /* HostCallAttachment v1 */
  put_u8(&route, 1);
  put_bytes(&route, (const uint8_t *)"/mnt/capi", strlen("/mnt/capi"));
  put_bytes(&route, (const uint8_t *)"native-test-driver", strlen("native-test-driver"));
  put_u16(&definition, 20); /* AttachmentDefinition v1 */
  put_u8(&definition, 1);
  put_u32(&definition, MC_HOST_ATTACHMENT_HOST_CALL);
  put_bytes(&definition, route.bytes, route.length);
  check(attachment_api.plan_add(runtime, plan,
                                (mc_host_bytes_v1){definition.bytes, definition.length}, NULL),
        "register native mount driver");
  wire persist = {0}, persist_definition = {0};
  put_u16(&persist, 34); /* PersistAttachment v1 */
  put_u8(&persist, 1);
  put_bytes(&persist, (const uint8_t *)"native-test-store", strlen("native-test-store"));
  put_u16(&persist_definition, 20);
  put_u8(&persist_definition, 1);
  put_u32(&persist_definition, MC_HOST_ATTACHMENT_PERSIST);
  put_bytes(&persist_definition, persist.bytes, persist.length);
  check(attachment_api.plan_add(runtime, plan,
                                (mc_host_bytes_v1){persist_definition.bytes, persist_definition.length}, NULL),
        "register native persistence provider");
  wire http = {0}, http_definition = {0};
  const char transport_name[] = "native-http-transport";
  mc_host_wire_http_application_attachment http_attachment = {
      .identity = {(const uint8_t *)transport_name, sizeof(transport_name) - 1}};
  need(mc_host_wire_encode_http_application_attachment(&http_attachment, http.bytes,
               sizeof(http.bytes), &http.length) == MC_HOST_WIRE_OK,
       "encode application HTTP attachment");
  mc_host_wire_attachment_definition http_entry = {
      .kind = MC_HOST_ATTACHMENT_HTTP_APPLICATION,
      .payload = {http.bytes, http.length}};
  need(mc_host_wire_encode_attachment_definition(&http_entry, http_definition.bytes,
               sizeof(http_definition.bytes), &http_definition.length) == MC_HOST_WIRE_OK,
       "encode attachment definition");
  check(attachment_api.plan_add(runtime, plan,
                                (mc_host_bytes_v1){http_definition.bytes, http_definition.length}, NULL),
        "register application HTTP transport");
  check(attachment_api.plan_seal(runtime, plan, NULL), "seal attachment plan");

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
  check(vm_api.vm_create(runtime, kernel_handle, plan, &create, &vm, NULL), "boot real image");
  need(api.runtime_close(runtime, NULL) == MC_HOST_BUSY, "runtime owns children");
  mc_host_vm_status_v1 status = MC_HOST_INIT(mc_host_vm_status_v1);
  need(vm_api.vm_status(runtime, kernel_handle, &status, NULL) == MC_HOST_WRONG_KIND,
       "kernel is not a VM");
  drive_to_prompt(&vm_api, &output_api, runtime, vm, NULL);
  run_control_command(&vm_api, &op_api, &snap_api, runtime, vm);
  run_filesystem_operations(&vm_api, &op_api, runtime, vm);
  run_layer_commit(&op_api, runtime, vm);
  run_autocomplete(&vm_api, &op_api, runtime, vm);
  run_service_call(&vm_api, &op_api, runtime, vm);
  run_mount_relay(&vm_api, &op_api, &effect_api, runtime, vm);
  run_persist_relay(&vm_api, &op_api, &effect_api, runtime, vm);
  run_http_relay(&vm_api, &op_api, &effect_api, runtime, vm);

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
  cancel_mount_relay(&vm_api, &op_api, &effect_api, runtime, vm);
  check(snap_api.baseline_close(runtime, baseline, NULL), "close baseline");
  check(vm_api.kernel_close(runtime, kernel_handle, NULL), "close kernel");
  check(attachment_api.plan_close(runtime, plan, NULL), "close attachment plan");
  check(api.runtime_close(runtime, NULL), "close runtime");
  free(delta);
  free(full);
  free(image);
  free(kernel);
  dlclose(lib);
  return 0;
}
