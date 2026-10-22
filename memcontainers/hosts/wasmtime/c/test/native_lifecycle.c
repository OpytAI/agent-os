#include "mc_host.h"

#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdatomic.h>
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

typedef struct status_close_probe {
  const mc_host_vm_api_v1 *api;
  mc_host_runtime_t runtime;
  mc_host_handle_t vm;
  _Atomic uint32_t observed;
  _Atomic uint32_t stale;
} status_close_probe;

static void *observe_until_closed(void *argument) {
  status_close_probe *probe = (status_close_probe *)argument;
  for (;;) {
    mc_host_vm_status_v1 status = MC_HOST_INIT(mc_host_vm_status_v1);
    mc_host_result_t result = probe->api->vm_status(probe->runtime, probe->vm,
                                                      &status, NULL);
    if (result == MC_HOST_OK) {
      atomic_store_explicit(&probe->observed, 1, memory_order_release);
      continue;
    }
    if (result == MC_HOST_BUSY) {
      sched_yield();
      continue;
    }
    need(result == MC_HOST_STALE_HANDLE, "concurrent status sees retired VM handle");
    atomic_store_explicit(&probe->stale, 1, memory_order_release);
    return NULL;
  }
}

typedef struct wake_gate {
  _Atomic uint32_t entered;
  _Atomic uint32_t release;
  _Atomic uint32_t calls;
} wake_gate;

static void MC_HOST_CALL count_wake(void *user_data) {
  wake_gate *gate = (wake_gate *)user_data;
  atomic_fetch_add_explicit(&gate->calls, 1, memory_order_relaxed);
  atomic_store_explicit(&gate->entered, 1, memory_order_release);
  while (!atomic_load_explicit(&gate->release, memory_order_acquire)) sched_yield();
}

static int contains_bytes(const uint8_t *haystack, size_t length,
                          const char *needle, size_t needle_length);
static void drive_to_prompt(const mc_host_vm_api_v1 *vm_api,
                            const mc_host_output_api_v1 *output_api,
                            mc_host_runtime_t runtime, mc_host_handle_t vm,
                            const char *expected);
static uint8_t *snapshot_bytes(const mc_host_snapshot_api_v1 *snap_api,
                               mc_host_runtime_t runtime, mc_host_handle_t snapshot,
                               uint64_t length);

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

static void *serve_native_connection_once(void *argument) {
  int listener = *(int *)argument;
  int peer = accept(listener, NULL, NULL);
  need(peer >= 0, "accept native connection request");
  uint8_t request[2048];
  size_t length = 0;
  while (!contains_bytes(request, length, "\r\n\r\n", 4)) {
    ssize_t count = recv(peer, request + length, sizeof(request) - length, 0);
    need(count > 0 && length + (size_t)count < sizeof(request),
         "read native connection request");
    length += (size_t)count;
  }
  need(contains_bytes(request, length, "GET /native-connection", 22) &&
       contains_bytes(request, length, "Bearer fixture-token", 20) &&
       !contains_bytes(request, length, "X-MC-Connection", 15),
       "credential is spliced after approval and marker is stripped");
  const char response[] = "HTTP/1.0 200 OK\r\nContent-Length: 6\r\n\r\nC-NET!";
  need(send(peer, response, sizeof(response) - 1, 0) == sizeof(response) - 1,
       "send native connection response");
  need(close(peer) == 0 && close(listener) == 0, "close native connection sockets");
  return NULL;
}

static void read_exact(int socket_fd, uint8_t *buffer, size_t length) {
  size_t offset = 0;
  while (offset < length) {
    ssize_t received = recv(socket_fd, buffer + offset, length - offset, 0);
    need(received > 0, "read complete WebSocket frame");
    offset += (size_t)received;
  }
}

static void *serve_ws_once(void *argument) {
  int listener = *(int *)argument;
  int peer = accept(listener, NULL, NULL);
  need(peer >= 0, "accept local WebSocket request");
  uint8_t request[1024];
  size_t length = 0;
  while (!contains_bytes(request, length, "\r\n\r\n", 4)) {
    ssize_t received = recv(peer, request + length, sizeof(request) - length, 0);
    need(received > 0 && length + (size_t)received < sizeof(request),
         "read bounded WebSocket handshake");
    length += (size_t)received;
  }
  need(contains_bytes(request, length, "GET /echo HTTP/1.1", 18) &&
       contains_bytes(request, length, "Upgrade: websocket", 18) &&
       contains_bytes(request, length, "dGhlIHNhbXBsZSBub25jZQ==", 24),
       "real WebSocket handshake request");
  const char response[] = "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\nConnection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
  need(send(peer, response, sizeof(response) - 1, 0) == sizeof(response) - 1,
       "send WebSocket handshake");
  uint8_t frame[10];
  read_exact(peer, frame, 6);
  need(frame[0] == 0x82 && frame[1] == 0x84, "masked binary WebSocket frame");
  read_exact(peer, frame + 6, 4);
  for (size_t index = 0; index < 4; ++index) frame[6 + index] ^= frame[2 + index];
  need(memcmp(frame + 6, "ping", 4) == 0, "real WebSocket frame payload");
  const uint8_t echo[] = {0x82, 4, 'p', 'i', 'n', 'g'};
  need(send(peer, echo, sizeof(echo), 0) == sizeof(echo), "send real WebSocket echo");
  need(close(peer) == 0 && close(listener) == 0, "close WebSocket sockets");
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
  mc_host_handle_t refused_fork = 0;
  need(vm_api->vm_fork(runtime, vm, &refused_fork, NULL) ==
           MC_HOST_NOT_QUIESCENT && refused_fork == 0,
       "fork refuses an unanswered host call");
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
  check(vm_api->vm_cancel_and_close(runtime, vm, NULL), "cancel VM with adopted effect");
  mc_host_vm_status_v1 closing = MC_HOST_INIT(mc_host_vm_status_v1);
  check(vm_api->vm_status(runtime, vm, &closing, NULL), "status of cancelled VM");
  need(closing.state == MC_HOST_VM_CLOSING, "cancelled VM stays CLOSING");
  need(vm_api->vm_close(runtime, vm, NULL) == MC_HOST_BUSY,
       "final close waits for the cancellation notice");
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
  check(vm_api->vm_close(runtime, vm, NULL), "finish closing source VM");
  need(op_api->poll(runtime, stat_op, &status, NULL) == MC_HOST_STALE_HANDLE,
       "final close retires the cancelled operation");
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
  mc_host_error_v1 error = MC_HOST_INIT(mc_host_error_v1);
  need(op_api->poll(runtime, operation, &status, &error) == MC_HOST_STALE_HANDLE &&
           error.operation_id == operation,
       "operation error identifies the failed operation");
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
      "assert(denied==nil); "
      "local cancelled=sys.net.fetch('http://127.0.0.1:%u/cancel'); "
      "assert(cancelled==nil); print('native-http-ok')",
      (unsigned)ntohs(address.sin_port),
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
      need(handled < 2 && info.kind == MC_HOST_EVENT_HTTP_REQUEST,
           "expected guest HTTP request");
      mc_host_effect_info_v1 inspected = MC_HOST_INIT(mc_host_effect_info_v1);
      check(effect_api->info(runtime, event, &inspected, NULL), "inspect leased HTTP event");
      need(inspected.kind == info.kind && inspected.length == info.length,
           "leased event identity and length");
      mc_host_effect_stats_v1 stats = MC_HOST_INIT(mc_host_effect_stats_v1);
      check(effect_api->stats(runtime, vm, &stats, NULL), "read live effect statistics");
      need(stats.leased_events >= 1 && stats.queued_bytes > 0,
           "leased effect remains inside queue quota");
      if (handled == 1) {
        wire payload = {0}, answer = {0};
        const char head[] = "200 OK\r\nContent-Length: 1\r\n\r\n";
        mc_host_wire_http_effect_response planned = {
            .failed = 0,
            .head = {(const uint8_t *)head, sizeof(head) - 1},
            .body = {NULL, 0}};
        need(mc_host_wire_encode_http_effect_response(&planned, payload.bytes,
                    sizeof(payload.bytes), &payload.length) == MC_HOST_WIRE_OK,
             "encode response later cancelled by application");
        mc_host_wire_effect_response answer_envelope = {
            .kind = MC_HOST_EVENT_HTTP_REQUEST,
            .event_id = (int64_t)event,
            .payload = {payload.bytes, payload.length}};
        need(mc_host_wire_encode_effect_response(&answer_envelope, answer.bytes,
                    sizeof(answer.bytes), &answer.length) == MC_HOST_WIRE_OK,
             "encode pending response before cancellation");
        check(effect_api->response_begin(runtime, event,
            (mc_host_bytes_v1){answer.bytes, answer.length}, 1, NULL),
            "begin response later cancelled by application");
        check(effect_api->cancel(runtime, event, NULL), "cancel second guest HTTP request");
        need(effect_api->info(runtime, event, &inspected, NULL) == MC_HOST_STALE_HANDLE,
             "cancelled event handle is stale");
        handled = 2;
        continue;
      }
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
      need(effect_api->response_begin(runtime, event,
            (mc_host_bytes_v1){answer.bytes, answer.length},
            64 * 1024, NULL) == MC_HOST_RESOURCE_LIMIT,
           "per-response limit rejects oversized real HTTP response");
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
  need(handled == 2 && status.state == MC_HOST_OP_READY && status.result_length < 4096,
       "guest HTTP fetch completes");
  uint8_t result[4096];
  mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, operation, &buffer, NULL), "read guest HTTP result");
  need(contains_bytes(result, buffer.required, "native-http-ok", 14),
       "real guest saw HTTP status");
  check(op_api->close(runtime, operation, NULL), "close guest HTTP fetch");
  need(pthread_join(server, NULL) == 0, "join real HTTP server");
}

static void run_effect_pressure(const mc_host_vm_api_v1 *vm_api,
                                const mc_host_operation_api_v1 *op_api,
                                const mc_host_effect_api_v1 *effect_api,
                                mc_host_runtime_t runtime, mc_host_handle_t vm) {
  const char script[] =
      "local sys=require('sys'); "
      "assert(sys.net.fetch('http://127.0.0.1:1/pressure')==nil); "
      "print('pressure-ok')";
  wire write = filesystem_request(MC_HOST_OP_FS_WRITE, "/tmp/native-pressure.luau",
                                  (const uint8_t *)script, sizeof(script) - 1);
  mc_host_handle_t write_op = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){write.bytes, write.length},
                      &write_op, NULL), "write guest effect pressure script");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 2048; ++attempt) {
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL), "drive pressure script write");
    check(op_api->poll(runtime, write_op, &status, NULL), "poll pressure script write");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(status.state == MC_HOST_OP_READY, "pressure script written");
  check(op_api->close(runtime, write_op, NULL), "close pressure script write");

  mc_host_handle_t jobs[9] = {0};
  wire command = shell_request("luau /tmp/native-pressure.luau");
  for (size_t i = 0; i < 9; ++i) {
    check(op_api->start(runtime, vm,
                        (mc_host_bytes_v1){command.bytes, command.length},
                        &jobs[i], NULL), "start concurrent guest HTTP pressure job");
  }
  mc_host_effect_stats_v1 stats = MC_HOST_INIT(mc_host_effect_stats_v1);
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL), "drive concurrent guest HTTP");
    check(effect_api->stats(runtime, vm, &stats, NULL), "inspect full effect queue");
    if (stats.refused_http > 0) break;
  }
  need(stats.refused_http > 0 && stats.queued_events == 8,
       "real guest exceeds configured eight-slot effect queue");
  mc_host_vm_status_v1 pressured = MC_HOST_INIT(mc_host_vm_status_v1);
  check(vm_api->vm_status(runtime, vm, &pressured, NULL), "status under effect pressure");
  need(pressured.active_effects == 8 &&
       (pressured.flags & MC_HOST_VM_FLAG_EFFECT_READY) != 0,
       "status reports unanswered effect requests");
  mc_host_drive_options_v1 held = MC_HOST_INIT(mc_host_drive_options_v1);
  held.max_ticks = 128;
  mc_host_drive_result_v1 held_drive = MC_HOST_INIT(mc_host_drive_result_v1);
  check(vm_api->vm_drive(runtime, vm, &held, &held_drive, NULL),
        "drive at effect high-water");
  need(held_drive.ticks == 0 &&
       held_drive.stop_reason == MC_HOST_DRIVE_STOP_EFFECT &&
       held_drive.status.active_effects == 8,
       "drive stops before ticking past the effect high-water mark");
  for (size_t i = 0; i < 8; ++i) {
    mc_host_handle_t event = 0;
    mc_host_effect_info_v1 info = MC_HOST_INIT(mc_host_effect_info_v1);
    check(effect_api->next(runtime, vm, UINT64_C(1) << MC_HOST_EVENT_HTTP_REQUEST,
                           &event, &info, NULL), "lease pressured HTTP request");
    need(info.kind == MC_HOST_EVENT_HTTP_REQUEST, "pressure event is guest HTTP");
    check(effect_api->cancel(runtime, event, NULL), "deny pressured HTTP request");
  }
  check(vm_api->vm_status(runtime, vm, &pressured, NULL), "status after denials");
  need(pressured.active_effects == 0, "denied requests no longer count as active");
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL), "complete pressure jobs");
    size_t ready = 0;
    for (size_t i = 0; i < 9; ++i) {
      check(op_api->poll(runtime, jobs[i], &status, NULL), "poll pressured guest job");
      ready += status.state == MC_HOST_OP_READY;
    }
    if (ready == 9) break;
  }
  for (size_t i = 0; i < 9; ++i) {
    check(op_api->poll(runtime, jobs[i], &status, NULL), "read pressure job status");
    need(status.state == MC_HOST_OP_READY && status.result_length < 4096,
         "pressured guest job completes");
    uint8_t result[4096];
    mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
    check(op_api->result(runtime, jobs[i], &buffer, NULL), "read pressure job result");
    need(contains_bytes(result, buffer.required, "pressure-ok", 11),
         "pressured guest observes network denial");
    check(op_api->close(runtime, jobs[i], NULL), "close pressure job");
  }
  uint64_t refused_before_large_event = stats.refused_http;
  const char large_script[] =
      "local sys=require('sys'); "
      "local url='http://127.0.0.1:1/'..string.rep('x',32768); "
      "assert(sys.net.fetch(url)==nil); print('large-event-refused')";
  wire large_write = filesystem_request(MC_HOST_OP_FS_WRITE,
      "/tmp/native-large-event.luau", (const uint8_t *)large_script,
      sizeof(large_script) - 1);
  mc_host_handle_t large_write_op = 0;
  check(op_api->start(runtime, vm,
        (mc_host_bytes_v1){large_write.bytes, large_write.length},
        &large_write_op, NULL), "write oversized guest event script");
  for (int attempt = 0; attempt < 2048; ++attempt) {
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL),
          "drive oversized event script write");
    check(op_api->poll(runtime, large_write_op, &status, NULL),
          "poll oversized event script write");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(status.state == MC_HOST_OP_READY, "oversized event script written");
  check(op_api->close(runtime, large_write_op, NULL),
        "close oversized event script write");
  command = shell_request("luau /tmp/native-large-event.luau");
  mc_host_handle_t large_operation = 0;
  check(op_api->start(runtime, vm,
        (mc_host_bytes_v1){command.bytes, command.length},
        &large_operation, NULL), "start oversized guest HTTP event");
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL),
          "drive oversized guest HTTP event");
    check(op_api->poll(runtime, large_operation, &status, NULL),
          "poll oversized guest HTTP event");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(status.state == MC_HOST_OP_READY && status.result_length < 4096,
       "oversized guest HTTP event returns to caller");
  uint8_t large_result[4096];
  mc_host_buffer_v1 large_buffer = {large_result, sizeof(large_result), 0};
  check(op_api->result(runtime, large_operation, &large_buffer, NULL),
        "read oversized guest HTTP event result");
  need(contains_bytes(large_result, large_buffer.required,
                      "large-event-refused", 19),
       "guest observes oversized event refusal");
  check(effect_api->stats(runtime, vm, &stats, NULL),
        "inspect oversized event refusal counter");
  need(stats.refused_http > refused_before_large_event && stats.queued_events == 0,
       "per-event byte ceiling refuses large guest request without queueing it");
  check(op_api->close(runtime, large_operation, NULL),
        "close oversized guest HTTP operation");
}

typedef struct stack_probe {
  const mc_host_vm_api_v1 *vm_api;
  const mc_host_operation_api_v1 *op_api;
  mc_host_runtime_t runtime;
  mc_host_handle_t vm;
} stack_probe;

static void stack_probe_wait(const stack_probe *probe, mc_host_handle_t operation,
                             const char *step) {
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(probe->vm_api->vm_drive(probe->runtime, probe->vm, &drive, &driven, NULL), step);
    check(probe->op_api->poll(probe->runtime, operation, &status, NULL), step);
    if (status.state == MC_HOST_OP_READY) return;
  }
  need(0, step);
}

static void stack_probe_command(const stack_probe *probe, const char *command,
                                const char *expected) {
  wire request = shell_request(command);
  mc_host_handle_t operation = 0;
  check(probe->op_api->start(probe->runtime, probe->vm,
                            (mc_host_bytes_v1){request.bytes, request.length},
                            &operation, NULL), "start guest command on eight-MiB thread");
  stack_probe_wait(probe, operation, "drive guest command on eight-MiB thread");
  if (expected) {
    uint8_t result[4096];
    mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
    check(probe->op_api->result(probe->runtime, operation, &buffer, NULL),
          "read post-depth guest command");
    need(contains_bytes(result, buffer.required, expected, strlen(expected)),
         "guest remains usable after pathological depth");
  }
  check(probe->op_api->close(probe->runtime, operation, NULL),
        "close guest command on eight-MiB thread");
}

static void *run_stack_probe(void *context) {
  const stack_probe *probe = (const stack_probe *)context;
  const size_t depth = 8000;
  const size_t capacity = 16 * depth + 128;
  uint8_t *source = malloc(capacity);
  need(source != NULL, "allocate pathological guest source");
  size_t length = 0;
#define APPEND_TEXT(value) do { \
    const char *part = (value); \
    size_t part_length = strlen(part); \
    need(length + part_length <= capacity, "bounded pathological source"); \
    memcpy(source + length, part, part_length); \
    length += part_length; \
  } while (0)
  APPEND_TEXT("--!strict\nlocal d: ");
  for (size_t i = 0; i < depth; ++i) APPEND_TEXT("{x:");
  APPEND_TEXT("number");
  for (size_t i = 0; i < depth; ++i) APPEND_TEXT("}");
  APPEND_TEXT(" = ");
  for (size_t i = 0; i < depth; ++i) APPEND_TEXT("{x=");
  APPEND_TEXT("false");
  for (size_t i = 0; i < depth; ++i) APPEND_TEXT("}");
  APPEND_TEXT("\nprint(d)\n");
#undef APPEND_TEXT
  mc_host_wire_filesystem_request file = {
      .path = {(const uint8_t *)"/tmp/native-deep.luau", 21},
      .has_data = 1,
      .data = {source, length}};
  size_t file_capacity = length + 128, file_length = 0;
  uint8_t *file_frame = malloc(file_capacity);
  need(file_frame != NULL &&
       mc_host_wire_encode_filesystem_request(&file, file_frame, file_capacity,
                                               &file_length) == MC_HOST_WIRE_OK,
       "encode pathological guest source write");
  mc_host_wire_operation_request operation_request = {
      .kind = MC_HOST_OP_FS_WRITE,
      .payload = {file_frame, file_length}};
  size_t frame_capacity = file_length + 128, frame_length = 0;
  uint8_t *frame = malloc(frame_capacity);
  need(frame != NULL &&
       mc_host_wire_encode_operation_request(&operation_request, frame, frame_capacity,
                                             &frame_length) == MC_HOST_WIRE_OK,
       "encode pathological guest write operation");
  mc_host_handle_t operation = 0;
  check(probe->op_api->start(probe->runtime, probe->vm,
                            (mc_host_bytes_v1){frame, frame_length}, &operation, NULL),
        "write pathological guest source on eight-MiB thread");
  free(frame);
  free(file_frame);
  free(source);
  stack_probe_wait(probe, operation, "drive pathological guest source write");
  check(probe->op_api->close(probe->runtime, operation, NULL),
        "close pathological source write");
  stack_probe_command(probe, "luau --check /tmp/native-deep.luau", NULL);
  stack_probe_command(probe, "luau -e 'print(2)'", "2");
  return NULL;
}

static void run_min_stack_probe(const mc_host_vm_api_v1 *vm_api,
                                const mc_host_operation_api_v1 *op_api,
                                mc_host_runtime_t runtime, mc_host_handle_t vm) {
  pthread_attr_t attr;
  need(pthread_attr_init(&attr) == 0, "initialize native stack attributes");
  need(pthread_attr_setstacksize(&attr, MC_HOST_MIN_NATIVE_STACK_BYTES) == 0,
       "set published minimum native stack");
  size_t stack_size = 0;
  need(pthread_attr_getstacksize(&attr, &stack_size) == 0 &&
       stack_size == MC_HOST_MIN_NATIVE_STACK_BYTES,
       "verify native stack for pathological guest");
  stack_probe probe = {vm_api, op_api, runtime, vm};
  pthread_t thread;
  need(pthread_create(&thread, &attr, run_stack_probe, &probe) == 0,
       "run pathological guest on minimum native stack");
  need(pthread_attr_destroy(&attr) == 0, "destroy native stack attributes");
  need(pthread_join(thread, NULL) == 0, "join minimum-stack guest thread");
}

static void run_native_connection(const mc_host_vm_api_v1 *vm_api,
                                  const mc_host_operation_api_v1 *op_api,
                                  const mc_host_effect_api_v1 *effect_api,
                                  const mc_host_snapshot_api_v1 *snap_api,
                                  const mc_host_output_api_v1 *output_api,
                                  const mc_host_attachment_api_v1 *attachment_api,
                                  mc_host_runtime_t runtime, mc_host_handle_t kernel,
                                  mc_host_vm_create_options_v1 *create) {
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  need(listener >= 0, "create native connection listener");
  struct sockaddr_in address = {0};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  need(bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0 &&
       listen(listener, 1) == 0, "listen for native connection");
  socklen_t address_length = sizeof(address);
  need(getsockname(listener, (struct sockaddr *)&address, &address_length) == 0,
       "read native connection port");
  pthread_t server;
  need(pthread_create(&server, NULL, serve_native_connection_once, &listener) == 0,
       "start native connection server");
  char origin_text[96];
  int origin_len = snprintf(origin_text, sizeof(origin_text), "http://127.0.0.1:%u",
                            (unsigned)ntohs(address.sin_port));
  need(origin_len > 0 && (size_t)origin_len < sizeof(origin_text), "native origin");

  mc_host_handle_t plan = 0;
  check(attachment_api->plan_create(runtime, &plan, NULL), "create native connection plan");
  wire origin = {0}, origins = {0}, payload = {0}, definition = {0};
  mc_host_wire_connection_origin origin_record = {
      .value = {(const uint8_t *)origin_text, (size_t)origin_len}};
  need(mc_host_wire_encode_connection_origin(&origin_record, origin.bytes,
           sizeof(origin.bytes), &origin.length) == MC_HOST_WIRE_OK,
       "encode allowed origin");
  put_bytes(&origins, origin.bytes, origin.length);
  const char reference[] = "test.org.main";
  const char bearer[] = "bearer";
  const char secret[] = "fixture-token";
  mc_host_wire_connection_attachment connection = {
      .reference = {(const uint8_t *)reference, sizeof(reference) - 1},
      .credential_kind = {(const uint8_t *)bearer, sizeof(bearer) - 1},
      .credential_a = {(const uint8_t *)secret, sizeof(secret) - 1},
      .credential_b = {NULL, 0},
      .origins = {origins.bytes, origins.length, 1}};
  need(mc_host_wire_encode_connection_attachment(&connection, payload.bytes,
           sizeof(payload.bytes), &payload.length) == MC_HOST_WIRE_OK,
       "encode native connection");
  mc_host_wire_attachment_definition entry = {
      .kind = MC_HOST_ATTACHMENT_CONNECTION,
      .payload = {payload.bytes, payload.length}};
  need(mc_host_wire_encode_attachment_definition(&entry, definition.bytes,
           sizeof(definition.bytes), &definition.length) == MC_HOST_WIRE_OK,
       "encode native connection attachment");
  check(attachment_api->plan_add(runtime, plan,
        (mc_host_bytes_v1){definition.bytes, definition.length}, NULL),
        "add native connection");
  const char owner[] = "org", pattern[] = "test.org.main.*";
  const char action[] = "require_approval";
  mc_host_wire_connection_policy_attachment policy = {
      .owner = {(const uint8_t *)owner, sizeof(owner) - 1},
      .pattern = {(const uint8_t *)pattern, sizeof(pattern) - 1},
      .action = {(const uint8_t *)action, sizeof(action) - 1}};
  payload.length = definition.length = 0;
  need(mc_host_wire_encode_connection_policy_attachment(&policy, payload.bytes,
           sizeof(payload.bytes), &payload.length) == MC_HOST_WIRE_OK,
       "encode native connection policy");
  entry.kind = MC_HOST_ATTACHMENT_CONNECTION_POLICY;
  entry.payload = (mc_host_wire_bytes){payload.bytes, payload.length};
  need(mc_host_wire_encode_attachment_definition(&entry, definition.bytes,
           sizeof(definition.bytes), &definition.length) == MC_HOST_WIRE_OK,
       "encode connection policy attachment");
  check(attachment_api->plan_add(runtime, plan,
        (mc_host_bytes_v1){definition.bytes, definition.length}, NULL),
        "add native connection policy");
  check(attachment_api->plan_seal(runtime, plan, NULL), "seal native connection plan");

  mc_host_vm_create_options_v1 native_create = *create;
  native_create.flags &= ~MC_HOST_VM_CREATE_DETERMINISTIC;
  mc_host_handle_t vm = 0;
  check(vm_api->vm_create(runtime, kernel, plan, &native_create, &vm, NULL),
        "boot native connection VM");
  drive_to_prompt(vm_api, output_api, runtime, vm, NULL);
  char script[512];
  int script_len = snprintf(script, sizeof(script),
      "local s=require('sys'); local u='%s/native-connection'; "
      "local o={headers={['X-MC-Connection']='test.org.main'}}; "
      "local a=assert(s.net.fetch(u,o)); assert(a.status==200); "
      "local b=assert(s.net.fetch(u,o)); assert(b.status==403); "
      "print('native-connection-ok')", origin_text);
  need(script_len > 0 && (size_t)script_len < sizeof(script), "native connection script");
  wire write = filesystem_request(MC_HOST_OP_FS_WRITE, "/tmp/native-connection.luau",
                                  (const uint8_t *)script, (size_t)script_len);
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){write.bytes, write.length},
                      &operation, NULL), "write native connection script");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 2048; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive native script write");
    check(op_api->poll(runtime, operation, &status, NULL), "poll native script write");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(status.state == MC_HOST_OP_READY, "native connection script written");
  check(op_api->close(runtime, operation, NULL), "close native script write");
  wire exec = shell_request("luau /tmp/native-connection.luau");
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){exec.bytes, exec.length},
                      &operation, NULL), "start native connection guest fetches");
  int approvals = 0;
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_EFFECT;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive native connection fetch");
    mc_host_handle_t event = 0;
    mc_host_effect_info_v1 info = MC_HOST_INIT(mc_host_effect_info_v1);
    mc_host_result_t next = effect_api->next(runtime, vm, 0, &event, &info, NULL);
    if (next == MC_HOST_OK) {
      need(approvals < 2 && info.kind == MC_HOST_EVENT_TOOL_APPROVAL,
           "native transport emits only approval facts");
      uint8_t frame[1024];
      mc_host_buffer_v1 buffer = {frame, sizeof(frame), 0};
      check(effect_api->read(runtime, event, &buffer, NULL), "read approval event");
      mc_host_wire_effect_event envelope;
      need(mc_host_wire_decode_effect_event(frame, buffer.required, &envelope) == MC_HOST_WIRE_OK,
           "decode approval envelope");
      mc_host_wire_tool_approval_request facts;
      need(mc_host_wire_decode_tool_approval_request(envelope.payload.data,
                 envelope.payload.len, &facts) == MC_HOST_WIRE_OK &&
           facts.connection.len == sizeof(reference) - 1 &&
           memcmp(facts.connection.data, reference, facts.connection.len) == 0 &&
           facts.origin.len == (size_t)origin_len &&
           memcmp(facts.origin.data, origin_text, facts.origin.len) == 0 &&
           !contains_bytes(frame, buffer.required, secret, sizeof(secret) - 1),
           "approval carries host facts without a credential");
      mc_host_snapshot_readiness_v1 readiness = MC_HOST_INIT(mc_host_snapshot_readiness_v1);
      check(snap_api->readiness(runtime, vm, &readiness, NULL),
            "readiness while approval unanswered");
      need(readiness.inflight_egress && readiness.pending_effects,
           "unanswered approval blocks snapshot");
      mc_host_wire_tool_approval_response decision = {
          .allow = (uint8_t)(approvals == 0), .remember_session = 0};
      wire answer = {0}, result = {0};
      need(mc_host_wire_encode_tool_approval_response(&decision, answer.bytes,
               sizeof(answer.bytes), &answer.length) == MC_HOST_WIRE_OK,
           "encode approval decision");
      mc_host_wire_effect_response response = {
          .kind = MC_HOST_EVENT_TOOL_APPROVAL, .event_id = (int64_t)event,
          .payload = {answer.bytes, answer.length}};
      need(mc_host_wire_encode_effect_response(&response, result.bytes,
               sizeof(result.bytes), &result.length) == MC_HOST_WIRE_OK,
           "encode approval effect response");
      check(effect_api->respond(runtime, event,
            (mc_host_bytes_v1){result.bytes, result.length}, NULL),
            "answer native connection approval");
      ++approvals;
    } else {
      need(next == MC_HOST_WOULD_BLOCK, "native approval queue");
    }
    check(op_api->poll(runtime, operation, &status, NULL), "poll native connection guest");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(approvals == 2 && status.state == MC_HOST_OP_READY && status.result_length < 4096,
       "native connection guest completes after allow and deny");
  uint8_t output[4096];
  mc_host_buffer_v1 result = {output, sizeof(output), 0};
  check(op_api->result(runtime, operation, &result, NULL), "read native connection guest result");
  need(contains_bytes(output, result.required, "native-connection-ok", 20),
       "guest observes native HTTP allow and decline");
  check(op_api->close(runtime, operation, NULL), "close native connection operation");
  need(pthread_join(server, NULL) == 0, "join native connection server");
  const char service_shard[] =
      "{\"annotations\":{},\"binding\":{\"adapter\":\"openapi\",\"args\":\"json\",\"op\":\"invoke\",\"request\":{\"method\":\"GET\",\"url_template\":\"http://example.test/native\"},\"service\":\"adapters\",\"type\":\"service\"}}";
  const char service_sha[] = "d8968bb007726cddf72264a09289584ceda7e2eb4d2aa0eeb30ae79341eaa5b9";
  const char service_index[] =
      "{\"generation\":1,\"tools\":[{\"address\":\"test.org.main.native\",\"description\":\"Native service\",\"integration\":\"test\",\"sha\":\"d8968bb007726cddf72264a09289584ceda7e2eb4d2aa0eeb30ae79341eaa5b9\"}]}";
  uint8_t record_bytes[512], list_bytes[600], apply_bytes[1200], request_bytes[1400];
  size_t record_len = 0, apply_len = 0, request_len = 0;
  mc_host_wire_catalog_record service_record = {
      .sha = {(const uint8_t *)service_sha, sizeof(service_sha) - 1},
      .bytes = {(const uint8_t *)service_shard, sizeof(service_shard) - 1}};
  need(mc_host_wire_encode_catalog_record(&service_record, record_bytes,
           sizeof(record_bytes), &record_len) == MC_HOST_WIRE_OK,
       "encode real service shard");
  mc_host_wire_list records = {0};
  need(mc_host_wire_list_append(&records, list_bytes, sizeof(list_bytes),
           (mc_host_wire_bytes){record_bytes, record_len}) == MC_HOST_WIRE_OK,
       "encode service catalog record list");
  mc_host_wire_catalog_apply_request apply = {
      .index_json = {(const uint8_t *)service_index, sizeof(service_index) - 1},
      .base_digest = {NULL, 0}, .records = records};
  need(mc_host_wire_encode_catalog_apply_request(&apply, apply_bytes,
           sizeof(apply_bytes), &apply_len) == MC_HOST_WIRE_OK,
       "encode service catalog transaction");
  mc_host_wire_operation_request catalog_request = {
      .kind = MC_HOST_OP_CATALOG_INJECT, .payload = {apply_bytes, apply_len}};
  need(mc_host_wire_encode_operation_request(&catalog_request, request_bytes,
           sizeof(request_bytes), &request_len) == MC_HOST_WIRE_OK,
       "encode service catalog operation");
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){request_bytes, request_len},
                      &operation, NULL), "start service catalog transaction");
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL),
          "drive service catalog transaction");
    check(op_api->poll(runtime, operation, &status, NULL),
          "poll service catalog transaction");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(status.state == MC_HOST_OP_READY, "real service catalog applies with connection");
  uint8_t catalog_output[4096];
  mc_host_buffer_v1 catalog_result = {catalog_output, sizeof(catalog_output), 0};
  check(op_api->result(runtime, operation, &catalog_result, NULL),
        "read service catalog result");
  need(contains_bytes(catalog_output, catalog_result.required, "\"ok\":true", 9),
       "service catalog committed in guest");
  check(op_api->close(runtime, operation, NULL), "close service catalog operation");
  mc_host_snapshot_begin_options_v1 capture = MC_HOST_INIT(mc_host_snapshot_begin_options_v1);
  capture.mode = MC_HOST_SNAPSHOT_FULL;
  mc_host_handle_t snapshot = 0;
  mc_host_snapshot_info_v1 snapshot_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api->begin(runtime, vm, &capture, &snapshot, &snapshot_info, NULL),
        "capture native connection VM");
  uint8_t *snapshot_data = snapshot_bytes(snap_api, runtime, snapshot, snapshot_info.length);
  mc_host_buffer_v1 manifest_probe = {NULL, 0, 0};
  need(snap_api->attachment_manifest(runtime, snapshot, &manifest_probe, NULL) ==
           MC_HOST_BUFFER_TOO_SMALL && manifest_probe.required > 0,
       "probe native connection manifest");
  uint8_t *manifest = malloc(manifest_probe.required);
  need(manifest != NULL, "allocate native connection manifest");
  mc_host_buffer_v1 manifest_buffer = {manifest, manifest_probe.required, 0};
  check(snap_api->attachment_manifest(runtime, snapshot, &manifest_buffer, NULL),
        "read native connection manifest");
  need(!contains_bytes(snapshot_data, snapshot_info.length, secret, sizeof(secret) - 1) &&
       !contains_bytes(manifest, manifest_buffer.required, secret, sizeof(secret) - 1),
       "snapshot and manifest omit connection credential");
  check(snap_api->finish(runtime, snapshot, NULL), "finish native connection snapshot");
  mc_host_vm_restore_options_v1 restore = MC_HOST_INIT(mc_host_vm_restore_options_v1);
  restore.snapshot = (mc_host_bytes_v1){snapshot_data, snapshot_info.length};
  restore.mode = MC_HOST_RESTORE_STRICT;
  mc_host_handle_t missing_plan = 0, rejected = 0;
  check(attachment_api->plan_create(runtime, &missing_plan, NULL),
        "create plan without service connection");
  check(attachment_api->plan_seal(runtime, missing_plan, NULL),
        "seal plan without service connection");
  need(vm_api->vm_restore_attached(runtime, kernel, missing_plan, &restore,
        (mc_host_bytes_v1){NULL, 0}, &rejected, NULL) == MC_HOST_INVALID_STATE &&
       rejected == 0, "strict restore refuses service catalog without connection");
  check(attachment_api->plan_close(runtime, missing_plan, NULL),
        "close missing connection plan");
  mc_host_handle_t restored = 0;
  check(vm_api->vm_restore_attached(runtime, kernel, plan, &restore,
        (mc_host_bytes_v1){manifest, manifest_buffer.required}, &restored, NULL),
        "strict restore reattaches native connection policy");
  check(vm_api->vm_close(runtime, restored, NULL), "close restored native connection VM");
  mc_host_handle_t child = 0;
  check(vm_api->vm_fork(runtime, vm, &child, NULL),
        "fork VM with native connection capability");
  check(vm_api->vm_close(runtime, child, NULL), "close native connection fork");
  free(snapshot_data);
  free(manifest);
  check(vm_api->vm_close(runtime, vm, NULL), "close native connection VM");
  check(attachment_api->plan_close(runtime, plan, NULL), "close native connection plan");
}

static void answer_ws(const mc_host_effect_api_v1 *effect_api,
                      mc_host_runtime_t runtime, mc_host_handle_t event,
                      uint32_t kind) {
  wire payload = {0}, answer = {0};
  if (kind == MC_HOST_EVENT_WS_CONNECT) {
    mc_host_wire_ws_connect_response response = {.failed = 0};
    need(mc_host_wire_encode_ws_connect_response(&response, payload.bytes,
             sizeof(payload.bytes), &payload.length) == MC_HOST_WIRE_OK,
         "encode WebSocket open");
  } else {
    mc_host_wire_ws_send_response response = {.failed = 0};
    need(mc_host_wire_encode_ws_send_response(&response, payload.bytes,
             sizeof(payload.bytes), &payload.length) == MC_HOST_WIRE_OK,
         "encode WebSocket send ack");
  }
  mc_host_wire_effect_response response = {
      .kind = kind, .event_id = (int64_t)event,
      .payload = {payload.bytes, payload.length}};
  need(mc_host_wire_encode_effect_response(&response, answer.bytes,
           sizeof(answer.bytes), &answer.length) == MC_HOST_WIRE_OK,
       "encode WebSocket effect answer");
  check(effect_api->respond(runtime, event,
        (mc_host_bytes_v1){answer.bytes, answer.length}, NULL),
        "answer WebSocket event");
}

static void run_ws_relay(const mc_host_vm_api_v1 *vm_api,
                         const mc_host_operation_api_v1 *op_api,
                         const mc_host_snapshot_api_v1 *snap_api,
                         const mc_host_effect_api_v1 *effect_api,
                         mc_host_runtime_t runtime, mc_host_handle_t vm) {
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  need(listener >= 0, "create WebSocket listener");
  struct sockaddr_in address = {0};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  need(bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0,
       "bind loopback WebSocket listener");
  need(listen(listener, 1) == 0, "listen for WebSocket request");
  socklen_t address_length = sizeof(address);
  need(getsockname(listener, (struct sockaddr *)&address, &address_length) == 0,
       "read WebSocket loopback port");
  pthread_t server;
  need(pthread_create(&server, NULL, serve_ws_once, &listener) == 0,
       "start real WebSocket server");

  char script[512];
  int script_length = snprintf(script, sizeof(script),
      "local sys=require('sys'); local fd=assert(sys.net.websocket('ws://127.0.0.1:%u/echo')); "
      "assert(sys.net.ws_send(fd,'ping')==4); "
      "assert(sys.net.ws_recv(fd)=='ping'); assert(sys.net.ws_close(fd)); "
      "print('native-ws-ok')", (unsigned)ntohs(address.sin_port));
  need(script_length > 0 && (size_t)script_length < sizeof(script),
       "guest WebSocket script");
  wire write = filesystem_request(MC_HOST_OP_FS_WRITE, "/tmp/native-ws.luau",
                                  (const uint8_t *)script, (size_t)script_length);
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){write.bytes, write.length},
                      &operation, NULL), "write guest WebSocket script");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 2048; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive WebSocket script write");
    check(op_api->poll(runtime, operation, &status, NULL), "poll WebSocket script write");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(status.state == MC_HOST_OP_READY, "guest WebSocket script written");
  check(op_api->close(runtime, operation, NULL), "close WebSocket script write");

  wire exec = shell_request("luau /tmp/native-ws.luau");
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){exec.bytes, exec.length},
                      &operation, NULL), "start guest WebSocket exchange");
  int client = -1, opened = 0, sent = 0, closed = 0;
  mc_host_handle_t socket_handle = 0;
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL),
          "drive guest WebSocket exchange");
    mc_host_handle_t event = 0;
    mc_host_effect_info_v1 info = MC_HOST_INIT(mc_host_effect_info_v1);
    uint64_t mask = (UINT64_C(1) << MC_HOST_EVENT_WS_CONNECT) |
                    (UINT64_C(1) << MC_HOST_EVENT_WS_SEND) |
                    (UINT64_C(1) << MC_HOST_EVENT_WS_CLOSE);
    mc_host_result_t next = effect_api->next(runtime, vm, mask, &event, &info, NULL);
    if (next == MC_HOST_OK) {
      uint8_t frame[2048];
      mc_host_buffer_v1 buffer = {frame, sizeof(frame), 0};
      check(effect_api->read(runtime, event, &buffer, NULL), "read WebSocket event");
      mc_host_wire_effect_event decoded;
      need(mc_host_wire_decode_effect_event(frame, buffer.required, &decoded) == MC_HOST_WIRE_OK &&
           decoded.event_id == (int64_t)event && decoded.has_socket_id,
           "WebSocket event envelope");
      if (info.kind == MC_HOST_EVENT_WS_CONNECT) {
        need(!opened && !sent && !closed, "first WebSocket event is connect");
        socket_handle = (mc_host_handle_t)decoded.socket_id;
        mc_host_wire_ws_connect_request request;
        need(mc_host_wire_decode_ws_connect_request(decoded.payload.data,
                 decoded.payload.len, &request) == MC_HOST_WIRE_OK &&
             contains_bytes(request.url.data, request.url.len, "/echo", 5),
             "guest WebSocket URL");
        client = socket(AF_INET, SOCK_STREAM, 0);
        need(client >= 0 && connect(client, (struct sockaddr *)&address,
                                   sizeof(address)) == 0,
             "connect application WebSocket transport");
        const char handshake[] = "GET /echo HTTP/1.1\r\nHost: 127.0.0.1\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n";
        need(send(client, handshake, sizeof(handshake) - 1, 0) == sizeof(handshake) - 1,
             "send application WebSocket handshake");
        uint8_t handshake_response[512];
        ssize_t received = recv(client, handshake_response, sizeof(handshake_response), 0);
        need(received > 0 && contains_bytes(handshake_response, (size_t)received,
                 "101 Switching Protocols", 23), "complete real WebSocket handshake");
        answer_ws(effect_api, runtime, event, info.kind);
        opened = 1;
      } else if (info.kind == MC_HOST_EVENT_WS_SEND) {
        need(opened && !sent && !closed && (mc_host_handle_t)decoded.socket_id == socket_handle,
             "WebSocket send shares socket identity");
        mc_host_wire_ws_send_request request;
        need(mc_host_wire_decode_ws_send_request(decoded.payload.data,
                 decoded.payload.len, &request) == MC_HOST_WIRE_OK &&
             request.data.len == 4 && memcmp(request.data.data, "ping", 4) == 0,
             "guest WebSocket send payload");
        const uint8_t mask_bytes[4] = {1, 2, 3, 4};
        uint8_t message[10] = {0x82, 0x84, 1, 2, 3, 4};
        for (size_t index = 0; index < 4; ++index)
          message[6 + index] = request.data.data[index] ^ mask_bytes[index];
        need(send(client, message, sizeof(message), 0) == sizeof(message),
             "send real WebSocket data frame");
        uint8_t echoed[6];
        read_exact(client, echoed, sizeof(echoed));
        need(echoed[0] == 0x82 && echoed[1] == 4 &&
             memcmp(echoed + 2, "ping", 4) == 0,
             "receive real WebSocket echo");
        check(effect_api->socket_push(runtime, socket_handle,
              (mc_host_bytes_v1){echoed + 2, 4}, NULL),
              "push echoed WebSocket data to guest");
        answer_ws(effect_api, runtime, event, info.kind);
        sent = 1;
      } else {
        need(info.kind == MC_HOST_EVENT_WS_CLOSE && opened && sent && !closed &&
             (mc_host_handle_t)decoded.socket_id == socket_handle,
             "guest WebSocket close event");
        mc_host_wire_ws_close_event close_event;
        need(mc_host_wire_decode_ws_close_event(decoded.payload.data,
                 decoded.payload.len, &close_event) == MC_HOST_WIRE_OK &&
             close_event.reason == MC_HOST_WS_CLOSE_GUEST, "WebSocket close reason");
        mc_host_snapshot_readiness_v1 readiness = MC_HOST_INIT(mc_host_snapshot_readiness_v1);
        check(snap_api->readiness(runtime, vm, &readiness, NULL),
              "inspect snapshot blockers with WebSocket close notice");
        need(!readiness.pending_effects,
             "close notice alone does not block a snapshot");
        check(effect_api->ack(runtime, event, NULL), "acknowledge WebSocket close");
        need(effect_api->socket_push(runtime, socket_handle,
             (mc_host_bytes_v1){(const uint8_t *)"x", 1}, NULL) == MC_HOST_STALE_HANDLE,
             "closed WebSocket handle is stale");
        need(close(client) == 0, "close application WebSocket transport");
        closed = 1;
      }
    } else {
      need(next == MC_HOST_WOULD_BLOCK, "WebSocket event queue");
    }
    check(op_api->poll(runtime, operation, &status, NULL), "poll guest WebSocket exchange");
    if (status.state == MC_HOST_OP_READY && closed) break;
  }
  need(opened && sent && closed && status.state == MC_HOST_OP_READY &&
       status.result_length < 4096, "guest WebSocket exchange completes");
  uint8_t result[4096];
  mc_host_buffer_v1 result_buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, operation, &result_buffer, NULL),
        "read guest WebSocket result");
  need(contains_bytes(result, result_buffer.required, "native-ws-ok", 12),
       "real guest received WebSocket echo");
  check(op_api->close(runtime, operation, NULL), "close WebSocket operation");
  need(pthread_join(server, NULL) == 0, "join real WebSocket server");
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

static void run_catalog_apply(const mc_host_vm_api_v1 *vm_api,
                              const mc_host_operation_api_v1 *op_api,
                              mc_host_runtime_t runtime, mc_host_handle_t vm) {
  const char shard[] =
      "{\"annotations\":{},\"binding\":{\"args\":\"json\",\"name\":\"native_echo\",\"type\":\"host_call\"}}";
  const char sha[] = "f175d3ceee11b5f90b52fdbda276a7cfbdf302720d960dff46dbfe6c84ce8bec";
  char index[512];
  for (unsigned generation = 1; generation <= 2; ++generation) {
    int index_length = snprintf(index, sizeof(index),
        "{\"generation\":%u,\"tools\":[{\"address\":\"native.org.capi.echo\","
        "\"description\":\"Native echo %u\",\"integration\":\"native\",\"sha\":\"%s\"}]}",
        generation, generation, sha);
    need(index_length > 0 && (size_t)index_length < sizeof(index),
         "catalog index fixture fits");
    uint8_t record_frame[256], list_frames[512], request_frame[1024], envelope[1200];
    size_t record_length = 0, request_length = 0, envelope_length = 0;
    mc_host_wire_catalog_record record = {
        .sha = {(const uint8_t *)sha, sizeof(sha) - 1},
        .bytes = {(const uint8_t *)shard, sizeof(shard) - 1}};
    need(mc_host_wire_encode_catalog_record(&record, record_frame,
             sizeof(record_frame), &record_length) == MC_HOST_WIRE_OK,
         "encode content-addressed catalog shard");
    mc_host_wire_list records = {0};
    need(mc_host_wire_list_append(&records, list_frames, sizeof(list_frames),
             (mc_host_wire_bytes){record_frame, record_length}) == MC_HOST_WIRE_OK,
         "encode catalog shard list");
    mc_host_wire_catalog_apply_request apply = {
        .index_json = {(const uint8_t *)index, (size_t)index_length},
        .base_digest = {(const uint8_t *)"", 0},
        .records = records};
    need(mc_host_wire_encode_catalog_apply_request(&apply, request_frame,
             sizeof(request_frame), &request_length) == MC_HOST_WIRE_OK,
         "encode catalog transaction");
    mc_host_wire_catalog_apply_request decoded_apply;
    need(mc_host_wire_decode_catalog_apply_request(request_frame, request_length,
             &decoded_apply) == MC_HOST_WIRE_OK && decoded_apply.records.count == 1,
         "decode catalog transaction");
    mc_host_wire_operation_request operation_request = {
        .kind = MC_HOST_OP_CATALOG_INJECT,
        .payload = {request_frame, request_length}};
    need(mc_host_wire_encode_operation_request(&operation_request, envelope,
             sizeof(envelope), &envelope_length) == MC_HOST_WIRE_OK,
         "encode catalog operation");
    mc_host_handle_t operation = 0;
    check(op_api->start(runtime, vm, (mc_host_bytes_v1){envelope, envelope_length},
                        &operation, NULL), "start catalog transaction");
    mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
    check(op_api->poll(runtime, operation, &status, NULL), "poll catalog before drive");
    need(status.state == MC_HOST_OP_PENDING, "catalog starts pending without guest effects");
    for (int attempt = 0; attempt < 8192; ++attempt) {
      mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
      options.max_ticks = 128;
      options.stop_mask = MC_HOST_DRIVE_ON_OPERATION;
      mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
      check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive catalog transaction");
      check(op_api->poll(runtime, operation, &status, NULL), "poll catalog transaction");
      if (status.state == MC_HOST_OP_READY) break;
      need(status.state == MC_HOST_OP_PENDING, "catalog transaction stays pending");
    }
    need(status.state == MC_HOST_OP_READY && status.result_length < 4096,
         "catalog transaction completes");
    uint8_t result[4096];
    mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
    check(op_api->result(runtime, operation, &buffer, NULL), "read catalog result");
    mc_host_wire_operation_result operation_result;
    need(mc_host_wire_decode_operation_result(result, buffer.required, &operation_result) ==
             MC_HOST_WIRE_OK && operation_result.kind == MC_HOST_OP_CATALOG_INJECT &&
             operation_result.status == 0,
         "catalog operation result envelope");
    mc_host_wire_svc_response response;
    need(mc_host_wire_decode_svc_response(operation_result.payload.data,
             operation_result.payload.len, &response) == MC_HOST_WIRE_OK &&
             response.status == 0,
         "real tools broker returned catalog result");
    if (generation == 1)
      need(contains_bytes(response.body.data, response.body.len, "\"ok\":true", 9),
           "catalog CAS committed");
    else
      need(contains_bytes(response.body.data, response.body.len, "catalog_conflict", 16),
           "stale catalog base rejected");
    check(op_api->close(runtime, operation, NULL), "close catalog transaction");
  }
  wire list = service_request("tools", "{\"op\":\"list\"}");
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){list.bytes, list.length},
                      &operation, NULL), "list committed catalog");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 options = MC_HOST_INIT(mc_host_drive_options_v1);
    options.max_ticks = 128;
    options.stop_mask = MC_HOST_DRIVE_ON_OPERATION;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &options, &driven, NULL), "drive committed catalog list");
    check(op_api->poll(runtime, operation, &status, NULL), "poll committed catalog list");
    if (status.state == MC_HOST_OP_READY) break;
  }
  need(status.state == MC_HOST_OP_READY && status.result_length < 4096,
       "committed catalog is readable");
  uint8_t result[4096];
  mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, operation, &buffer, NULL), "read committed catalog list");
  mc_host_wire_operation_result operation_result;
  mc_host_wire_svc_response response;
  need(mc_host_wire_decode_operation_result(result, buffer.required, &operation_result) ==
           MC_HOST_WIRE_OK &&
           mc_host_wire_decode_svc_response(operation_result.payload.data,
               operation_result.payload.len, &response) == MC_HOST_WIRE_OK &&
           contains_bytes(response.body.data, response.body.len,
               "native.org.capi.echo", strlen("native.org.capi.echo")),
       "committed catalog lists native tool");
  check(op_api->close(runtime, operation, NULL), "close catalog list");
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
  mc_host_snapshot_readiness_v1 readiness = MC_HOST_INIT(mc_host_snapshot_readiness_v1);
  check(snap_api->readiness(runtime, vm, &readiness, NULL),
        "inspect snapshot blockers with live control operation");
  need(readiness.pending_operations && !readiness.pending_transactions,
       "readiness reports live control operation separately");
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
  check(vm_api->vm_cancel_and_close(runtime, vm, NULL), "cancel VM with operation");
  need(op_api->start(runtime, vm, (mc_host_bytes_v1){request.bytes, request.length},
                     &(mc_host_handle_t){0}, NULL) == MC_HOST_INVALID_STATE,
       "CLOSING VM admits no new operation");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  check(op_api->poll(runtime, operation, &status, NULL), "observe cancelled operation");
  need(status.state == MC_HOST_OP_CANCELLED && status.error == MC_HOST_CANCELLED,
       "operation reports cancellation");
  mc_host_buffer_v1 result = {0};
  need(op_api->result(runtime, operation, &result, NULL) == MC_HOST_CANCELLED,
       "cancelled operation has no result");
  check(vm_api->vm_close(runtime, vm, NULL), "finish closing cancelled VM");
  need(op_api->poll(runtime, operation, &status, NULL) == MC_HOST_STALE_HANDLE,
       "final close retires the cancelled operation");
  need(vm_api->vm_close(runtime, vm, NULL) == MC_HOST_STALE_HANDLE,
       "closed VM handle is stale");
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

static void verify_rust_host_snapshot(const mc_host_vm_api_v1 *vm_api,
                                      const mc_host_operation_api_v1 *op_api,
                                      const mc_host_snapshot_api_v1 *snap_api,
                                      mc_host_runtime_t runtime, mc_host_handle_t kernel,
                                      const uint8_t *snapshot, size_t snapshot_length,
                                      const uint8_t *baseline, size_t baseline_length) {
  mc_host_snapshot_info_v1 info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api->inspect(runtime,
        (mc_host_bytes_v1){snapshot, snapshot_length},
        (mc_host_bytes_v1){baseline, baseline_length}, &info, NULL),
        "inspect Rust-host snapshot under C API");
  need(info.kind == (baseline_length ? MC_HOST_SNAPSHOT_INCREMENTAL : MC_HOST_SNAPSHOT_FULL) &&
       info.length == snapshot_length, "cross-host snapshot metadata");

  mc_host_vm_restore_options_v1 options = MC_HOST_INIT(mc_host_vm_restore_options_v1);
  options.snapshot = (mc_host_bytes_v1){snapshot, snapshot_length};
  options.baseline = (mc_host_bytes_v1){baseline, baseline_length};
  options.mode = MC_HOST_RESTORE_DETACHED;
  mc_host_handle_t vm = 0;
  check(vm_api->vm_restore(runtime, kernel, 0, &options, &vm, NULL),
        "restore Rust-host snapshot in C API");

  wire read = filesystem_request(MC_HOST_OP_FS_READ, "/tmp/xhost", NULL, 0);
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){read.bytes, read.length},
                      &operation, NULL), "read Rust-host guest marker");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 2048; ++attempt) {
    check(op_api->poll(runtime, operation, &status, NULL),
          "poll cross-host filesystem read");
    if (status.state == MC_HOST_OP_READY) break;
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL),
          "drive restored Rust-host guest");
  }
  need(status.state == MC_HOST_OP_READY && status.result_length < 256,
       "cross-host filesystem read completes");
  uint8_t result[256];
  mc_host_buffer_v1 buffer = {result, sizeof(result), 0};
  check(op_api->result(runtime, operation, &buffer, NULL),
        "read cross-host filesystem result");
  mc_host_wire_operation_result decoded;
  const char marker[] = "rust-host snapshot -> js-host restore";
  need(mc_host_wire_decode_operation_result(result, buffer.required, &decoded) ==
           MC_HOST_WIRE_OK && decoded.kind == MC_HOST_OP_FS_READ &&
           decoded.status == 0 && decoded.payload.len == sizeof(marker) - 1 &&
           memcmp(decoded.payload.data, marker, sizeof(marker) - 1) == 0,
       "Rust-host guest file survives C restore");
  check(op_api->close(runtime, operation, NULL), "close cross-host read");
  check(vm_api->vm_close(runtime, vm, NULL), "close cross-host restored VM");
}

static void verify_identical_capture(const mc_host_vm_api_v1 *vm_api,
                                     const mc_host_operation_api_v1 *op_api,
                                     const mc_host_snapshot_api_v1 *snap_api,
                                     mc_host_runtime_t runtime, mc_host_handle_t kernel,
                                     const uint8_t *base_image, size_t base_image_length,
                                     const uint8_t *rust_snapshot, size_t rust_snapshot_length) {
  mc_host_bytes_v1 layer = {base_image, base_image_length};
  mc_host_vm_create_options_v1 create = MC_HOST_INIT(mc_host_vm_create_options_v1);
  create.layers = &layer;
  create.layer_count = 1;
  create.requested_workers = -1;
  create.flags = MC_HOST_VM_CREATE_DETERMINISTIC;
  mc_host_handle_t vm = 0;
  check(vm_api->vm_create(runtime, kernel, 0, &create, &vm, NULL),
        "boot deterministic base image under C API");
  mc_host_vm_status_v1 status = MC_HOST_INIT(mc_host_vm_status_v1);
  for (int attempt = 0; attempt < 8192; ++attempt) {
    check(vm_api->vm_status(runtime, vm, &status, NULL),
          "observe deterministic guest prompt");
    if (status.flags & MC_HOST_VM_FLAG_AT_PROMPT) break;
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 1;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL),
          "drive deterministic base image one tick");
  }
  need(status.flags & MC_HOST_VM_FLAG_AT_PROMPT,
       "deterministic base image reaches the same prompt");
  const char marker[] = "rust-host snapshot -> js-host restore";
  wire write = filesystem_request(MC_HOST_OP_FS_WRITE, "/tmp/xhost",
                                  (const uint8_t *)marker, sizeof(marker) - 1);
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){write.bytes, write.length},
                      &operation, NULL), "write deterministic cross-host marker");
  mc_host_operation_status_v1 write_status = MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 32; ++attempt) {
    check(op_api->poll(runtime, operation, &write_status, NULL),
          "poll deterministic marker write");
    if (write_status.state == MC_HOST_OP_READY) break;
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 1;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL),
          "advance deterministic marker write");
  }
  need(write_status.state == MC_HOST_OP_READY,
       "local deterministic write completes");
  check(op_api->close(runtime, operation, NULL),
        "close deterministic marker write");

  mc_host_snapshot_begin_options_v1 capture = MC_HOST_INIT(mc_host_snapshot_begin_options_v1);
  capture.mode = MC_HOST_SNAPSHOT_FULL;
  mc_host_handle_t snapshot = 0;
  mc_host_snapshot_info_v1 info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api->begin(runtime, vm, &capture, &snapshot, &info, NULL),
        "capture deterministic C snapshot");
  need(info.length == rust_snapshot_length,
       "C and Rust snapshots have the same length");
  uint8_t *c_snapshot = snapshot_bytes(snap_api, runtime, snapshot, info.length);
  need(memcmp(c_snapshot, rust_snapshot, rust_snapshot_length) == 0,
       "C and Rust snapshot bytes are identical");
  free(c_snapshot);
  check(snap_api->finish(runtime, snapshot, NULL),
        "finish deterministic C capture");
  check(vm_api->vm_close(runtime, vm, NULL),
        "close deterministic C VM");
}

static void verify_operation_result_quota(const mc_host_api_v1 *api,
                                          const mc_host_vm_api_v1 *vm_api,
                                          const mc_host_operation_api_v1 *op_api,
                                          const mc_host_output_api_v1 *output_api,
                                          const uint8_t *kernel, size_t kernel_len,
                                          const uint8_t *image, size_t image_len) {
  mc_host_runtime_options_v1 options = MC_HOST_INIT(mc_host_runtime_options_v1);
  options.max_operation_result_bytes = 64;
  options.max_message_bytes = 128;
  options.max_output_bytes = 1024;
  options.max_output_records = 4096;
  mc_host_runtime_t runtime = 0;
  check(api->runtime_create(&options, &runtime, NULL), "create result-quota runtime");
  mc_host_handle_t kernel_handle = 0;
  mc_host_kernel_info_v1 kernel_info = MC_HOST_INIT(mc_host_kernel_info_v1);
  check(vm_api->kernel_load(runtime, (mc_host_bytes_v1){kernel, kernel_len},
                            &kernel_handle, &kernel_info, NULL), "load quota kernel");
  mc_host_bytes_v1 layer = {image, image_len};
  mc_host_vm_create_options_v1 create = MC_HOST_INIT(mc_host_vm_create_options_v1);
  create.layers = &layer;
  create.layer_count = 1;
  create.requested_workers = -1;
  mc_host_handle_t vm = 0;
  check(vm_api->vm_create(runtime, kernel_handle, 0, &create, &vm, NULL),
        "boot real base image under result quota");
  drive_to_prompt(vm_api, output_api, runtime, vm, NULL);
  char oversized_command[160];
  memset(oversized_command, 'x', sizeof(oversized_command) - 1);
  oversized_command[sizeof(oversized_command) - 1] = '\0';
  wire oversized_request = shell_request(oversized_command);
  mc_host_handle_t rejected_operation = 0;
  need(op_api->start(runtime, vm,
           (mc_host_bytes_v1){oversized_request.bytes, oversized_request.length},
           &rejected_operation, NULL) == MC_HOST_RESOURCE_LIMIT &&
           rejected_operation == 0,
       "oversized encoded request is rejected before guest execution");
  wire request = shell_request("git --version; git --version; git --version; git --version");
  mc_host_handle_t operation = 0;
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){request.bytes, request.length},
                      &operation, NULL), "start real command under result quota");
  mc_host_operation_status_v1 status = MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    drive.stop_mask = MC_HOST_DRIVE_ON_OPERATION;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL),
          "drive real command under result quota");
    check(op_api->poll(runtime, operation, &status, NULL),
          "poll real command under result quota");
    if (status.state != MC_HOST_OP_PENDING) break;
  }
  need(status.state == MC_HOST_OP_FAILED && status.error == MC_HOST_RESOURCE_LIMIT &&
           status.result_length == 0,
       "oversized real command result fails without truncation");
  check(op_api->close(runtime, operation, NULL), "close failed quota operation");
  request = shell_request("echo x");
  check(op_api->start(runtime, vm, (mc_host_bytes_v1){request.bytes, request.length},
                      &operation, NULL), "start small real command under result quota");
  status = (mc_host_operation_status_v1)MC_HOST_INIT(mc_host_operation_status_v1);
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    drive.stop_mask = MC_HOST_DRIVE_ON_OPERATION;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL),
          "drive small real command under result quota");
    check(op_api->poll(runtime, operation, &status, NULL),
          "poll small real command under result quota");
    if (status.state != MC_HOST_OP_PENDING) break;
  }
  need(status.state == MC_HOST_OP_READY && status.result_length <= 64,
       "small real command remains readable under result quota");
  check(op_api->close(runtime, operation, NULL), "close small quota operation");
  const uint8_t command[] = "echo quota-survived\n";
  check(vm_api->terminal_input(runtime, vm,
          (mc_host_bytes_v1){command, sizeof(command) - 1}, NULL),
        "send terminal command after result quota failure");
  drive_to_prompt(vm_api, output_api, runtime, vm, "quota-survived");
  uint8_t wide_command[2048];
  memcpy(wide_command, "echo ", 5);
  memset(wide_command + 5, 'x', sizeof(wide_command) - 6);
  wide_command[sizeof(wide_command) - 1] = '\n';
  check(vm_api->terminal_input(runtime, vm,
        (mc_host_bytes_v1){wide_command, sizeof(wide_command)}, NULL),
        "send large real terminal command into bounded output queue");
  int saw_gap = 0, returned_to_prompt = 0;
  for (int attempt = 0; attempt < 8192; ++attempt) {
    mc_host_drive_options_v1 drive = MC_HOST_INIT(mc_host_drive_options_v1);
    drive.max_ticks = 128;
    mc_host_drive_result_v1 driven = MC_HOST_INIT(mc_host_drive_result_v1);
    check(vm_api->vm_drive(runtime, vm, &drive, &driven, NULL),
          "drive large terminal output under quota");
    for (;;) {
      mc_host_handle_t lease = 0;
      mc_host_output_record_v1 record = MC_HOST_INIT(mc_host_output_record_v1);
      mc_host_result_t next = output_api->next(runtime, vm, &lease, &record, NULL);
      if (next == MC_HOST_WOULD_BLOCK) break;
      check(next, "lease pressured terminal output");
      if (record.kind == MC_HOST_OUTPUT_GAP) {
        need(record.length > 0 && record.byte_end > record.byte_offset &&
             record.first_lost_sequence <= record.last_lost_sequence,
             "terminal gap identifies lost real guest bytes");
        saw_gap = 1;
      }
      check(output_api->ack(runtime, lease, NULL),
            "ack pressured terminal output");
    }
    need(driven.status.state != MC_HOST_VM_POISONED &&
         driven.status.state != MC_HOST_VM_EXITED,
         "guest remains alive after terminal pressure");
    if (driven.status.flags & MC_HOST_VM_FLAG_AT_PROMPT) {
      returned_to_prompt = 1;
      break;
    }
  }
  mc_host_output_stats_v1 output_stats = MC_HOST_INIT(mc_host_output_stats_v1);
  check(output_api->stats(runtime, vm, &output_stats, NULL),
        "inspect terminal output loss statistics");
  need(saw_gap && returned_to_prompt && output_stats.dropped_bytes > 0 &&
       !(output_stats.flags & MC_HOST_OUTPUT_POISONED),
       "single large guest command records bounded output loss");
  check(vm_api->vm_close(runtime, vm, NULL), "close quota VM");
  check(vm_api->kernel_close(runtime, kernel_handle, NULL), "close quota kernel");
  check(api->runtime_close(runtime, NULL), "close result-quota runtime");
}

static void verify_effect_transcript(const mc_host_effect_api_v1 *api,
                                     mc_host_runtime_t runtime, mc_host_handle_t vm) {
  int requests = 0, responses = 0, inbound = 0;
  int64_t previous = 0;
  for (;;) {
    mc_host_buffer_v1 buffer = {0};
    mc_host_result_t status = api->transcript_next(runtime, vm, &buffer, NULL);
    if (status == MC_HOST_WOULD_BLOCK) break;
    need(status == MC_HOST_BUFFER_TOO_SMALL && buffer.required > 0,
         "probe real effect transcript without consuming it");
    uint8_t *bytes = malloc((size_t)buffer.required);
    need(bytes != NULL, "allocate bounded transcript read");
    buffer.data = bytes;
    buffer.capacity = buffer.required;
    check(api->transcript_next(runtime, vm, &buffer, NULL),
          "read real effect transcript entry");
    mc_host_wire_effect_transcript_entry entry;
    need(mc_host_wire_decode_effect_transcript_entry(bytes, (size_t)buffer.required,
             &entry) == MC_HOST_WIRE_OK && entry.sequence == previous + 1,
         "ordered generated transcript envelope");
    previous = entry.sequence;
    if (entry.direction == MC_HOST_TRANSCRIPT_REQUEST &&
        entry.kind == MC_HOST_EVENT_HOST_CALL) ++requests;
    if (entry.direction == MC_HOST_TRANSCRIPT_RESPONSE &&
        entry.kind == MC_HOST_EVENT_HOST_CALL) ++responses;
    if (entry.direction == MC_HOST_TRANSCRIPT_WS_INBOUND) ++inbound;
    check(api->transcript_ack(runtime, vm, (uint64_t)entry.sequence, NULL),
          "ack real effect transcript entry");
    free(bytes);
  }
  need(requests > 0 && responses > 0 && inbound > 0,
       "real host calls and inbound WebSocket data are recorded");
}

int main(int argc, char **argv) {
  need(argc == 7, "release library, kernel, images, and snapshot fixtures");
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
  mc_host_runtime_options_v1 runtime_options = MC_HOST_INIT(mc_host_runtime_options_v1);
  runtime_options.max_vms = 2;
  runtime_options.max_drive_ticks = 128;
  runtime_options.max_output_bytes = 1024 * 1024;
  runtime_options.max_output_records = 1024;
  runtime_options.max_snapshot_bytes = 480 * 1024 * 1024;
  runtime_options.max_snapshot_total_bytes = 900 * 1024 * 1024;
  runtime_options.max_operations_per_vm = 16;
  runtime_options.max_effect_events = 8;
  runtime_options.max_effect_bytes = 64 * 1024;
  runtime_options.max_effect_response_bytes = 1024 * 1024;
  runtime_options.max_effect_event_bytes = 32 * 1024;
  runtime_options.max_effect_single_response_bytes = 64 * 1024;
  runtime_options.max_ws_sockets = 4;
  runtime_options.max_ws_incoming_bytes = 128 * 1024;
  need(kernel_len > 1 && kernel_len <= UINT32_MAX && image_len <= UINT32_MAX,
       "real artifacts fit native runtime quota fields");
  runtime_options.max_kernels = 1;
  runtime_options.max_kernel_bytes = (uint32_t)kernel_len;
  runtime_options.max_compiled_modules = 1;
  runtime_options.max_image_bytes = (uint32_t)image_len;
  runtime_options.max_snapshot_handles = 8;
  wake_gate wake = {0};
  runtime_options.wake = count_wake;
  runtime_options.wake_user_data = &wake;
  check(api.runtime_create(&runtime_options, &runtime, NULL), "create bounded runtime");

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
  wire tool_route = {0}, tool_definition = {0};
  put_u16(&tool_route, 29);
  put_u8(&tool_route, 1);
  put_bytes(&tool_route, (const uint8_t *)"native_echo", strlen("native_echo"));
  put_bytes(&tool_route, (const uint8_t *)"native-tool-handler", strlen("native-tool-handler"));
  put_u16(&tool_definition, 20);
  put_u8(&tool_definition, 1);
  put_u32(&tool_definition, MC_HOST_ATTACHMENT_HOST_CALL);
  put_bytes(&tool_definition, tool_route.bytes, tool_route.length);
  check(attachment_api.plan_add(runtime, plan,
                                (mc_host_bytes_v1){tool_definition.bytes, tool_definition.length}, NULL),
        "register native tool handler");
  wire host_tool = {0}, host_tool_definition = {0};
  mc_host_wire_host_tool_attachment host_tool_spec = {
      .address = {(const uint8_t *)"native.org.capi.echo", strlen("native.org.capi.echo")},
      .description = {(const uint8_t *)"Native echo 1", strlen("Native echo 1")},
      .binding_name = {(const uint8_t *)"native_echo", strlen("native_echo")},
      .args_mode = {(const uint8_t *)"json", strlen("json")}};
  need(mc_host_wire_encode_host_tool_attachment(&host_tool_spec, host_tool.bytes,
           sizeof(host_tool.bytes), &host_tool.length) == MC_HOST_WIRE_OK,
       "encode native host tool attachment");
  mc_host_wire_attachment_definition host_tool_entry = {
      .kind = MC_HOST_ATTACHMENT_HOST_TOOL,
      .payload = {host_tool.bytes, host_tool.length}};
  need(mc_host_wire_encode_attachment_definition(&host_tool_entry,
           host_tool_definition.bytes, sizeof(host_tool_definition.bytes),
           &host_tool_definition.length) == MC_HOST_WIRE_OK,
       "encode host tool definition");
  check(attachment_api.plan_add(runtime, plan,
                                (mc_host_bytes_v1){host_tool_definition.bytes,
                                                   host_tool_definition.length}, NULL),
        "register native host tool definition");
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
  mc_host_handle_t excess_kernel = 0;
  need(vm_api.kernel_load(runtime, (mc_host_bytes_v1){kernel, kernel_len},
                          &excess_kernel, &kernel_info, NULL) == MC_HOST_RESOURCE_LIMIT &&
       excess_kernel == 0, "configured kernel handle quota rejects a second real kernel");
  mc_host_runtime_options_v1 byte_limited = MC_HOST_INIT(mc_host_runtime_options_v1);
  byte_limited.max_kernel_bytes = (uint32_t)kernel_len - 1;
  mc_host_runtime_t byte_runtime = 0;
  check(api.runtime_create(&byte_limited, &byte_runtime, NULL),
        "create runtime below real kernel size");
  need(vm_api.kernel_load(byte_runtime, (mc_host_bytes_v1){kernel, kernel_len},
                          &excess_kernel, &kernel_info, NULL) == MC_HOST_RESOURCE_LIMIT &&
       excess_kernel == 0, "kernel byte quota rejects real artifact before compile");
  check(api.runtime_close(byte_runtime, NULL), "close kernel byte quota runtime");
  mc_host_bytes_v1 layer = {image, image_len};
  mc_host_vm_create_options_v1 create = MC_HOST_INIT(mc_host_vm_create_options_v1);
  create.layers = &layer;
  create.layer_count = 1;
  create.requested_workers = -1;
  create.flags = MC_HOST_VM_CREATE_DETERMINISTIC;
  mc_host_handle_t vm = 0;
  check(vm_api.vm_create(runtime, kernel_handle, plan, &create, &vm, NULL), "boot real image");
  mc_host_drive_options_v1 over_limit = MC_HOST_INIT(mc_host_drive_options_v1);
  over_limit.max_ticks = 129;
  mc_host_drive_result_v1 rejected_drive = MC_HOST_INIT(mc_host_drive_result_v1);
  need(vm_api.vm_drive(runtime, vm, &over_limit, &rejected_drive, NULL) ==
           MC_HOST_INVALID_ARGUMENT, "runtime drive limit is enforced");
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
  run_catalog_apply(&vm_api, &op_api, runtime, vm);
  run_mount_relay(&vm_api, &op_api, &effect_api, runtime, vm);
  run_persist_relay(&vm_api, &op_api, &effect_api, runtime, vm);
  run_http_relay(&vm_api, &op_api, &effect_api, runtime, vm);
  run_ws_relay(&vm_api, &op_api, &snap_api, &effect_api, runtime, vm);
  verify_effect_transcript(&effect_api, runtime, vm);

  mc_host_snapshot_readiness_v1 readiness = MC_HOST_INIT(mc_host_snapshot_readiness_v1);
  check(snap_api.readiness(runtime, vm, &readiness, NULL),
        "inspect quiescent VM before snapshot");
  need(!readiness.inflight_egress && !readiness.pending_commits &&
       !readiness.pending_operations && !readiness.pending_effects &&
       !readiness.pending_transactions,
       "snapshot readiness reports no remaining blockers");

  wire snapshot_mount = mount_request(MC_HOST_OP_MOUNT, "/mnt/capi");
  mc_host_handle_t snapshot_mount_op = 0;
  check(op_api.start(runtime, vm,
                     (mc_host_bytes_v1){snapshot_mount.bytes, snapshot_mount.length},
                     &snapshot_mount_op, NULL), "mount host driver before snapshot");
  check(op_api.close(runtime, snapshot_mount_op, NULL),
        "close pre-snapshot mount operation");
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

  need(full_info.length > 1 && full_info.length <= UINT32_MAX,
       "real snapshot supports a tighter runtime quota");
  mc_host_runtime_options_v1 tight_options = MC_HOST_INIT(mc_host_runtime_options_v1);
  tight_options.max_snapshot_bytes = (uint32_t)full_info.length - 1;
  tight_options.max_image_bytes = 1;
  mc_host_runtime_t tight_runtime = 0;
  check(api.runtime_create(&tight_options, &tight_runtime, NULL),
        "create runtime with snapshot quota below a real capture");
  mc_host_handle_t tight_kernel = 0;
  mc_host_kernel_info_v1 tight_kernel_info = MC_HOST_INIT(mc_host_kernel_info_v1);
  check(vm_api.kernel_load(tight_runtime, (mc_host_bytes_v1){kernel, kernel_len},
                           &tight_kernel, &tight_kernel_info, NULL),
        "load real kernel into quota runtime");
  mc_host_handle_t tight_image_vm = 0;
  need(vm_api.vm_create(tight_runtime, tight_kernel, 0, &create,
                        &tight_image_vm, NULL) == MC_HOST_RESOURCE_LIMIT &&
       tight_image_vm == 0, "image quota rejects real layer before copy");
  mc_host_handle_t tight_baseline = 0;
  mc_host_snapshot_info_v1 tight_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  need(snap_api.baseline_import(tight_runtime, tight_kernel,
                                (mc_host_bytes_v1){full, full_info.length},
                                &tight_baseline, &tight_info, NULL) == MC_HOST_RESOURCE_LIMIT &&
           tight_baseline == 0,
       "runtime snapshot quota rejects real baseline import");
  mc_host_vm_restore_options_v1 tight_restore = MC_HOST_INIT(mc_host_vm_restore_options_v1);
  tight_restore.snapshot = (mc_host_bytes_v1){full, full_info.length};
  tight_restore.mode = MC_HOST_RESTORE_DETACHED;
  mc_host_handle_t tight_vm = 0;
  need(vm_api.vm_restore(tight_runtime, tight_kernel, 0, &tight_restore,
                         &tight_vm, NULL) == MC_HOST_RESOURCE_LIMIT && tight_vm == 0,
       "runtime snapshot quota rejects real restore");
  check(vm_api.kernel_close(tight_runtime, tight_kernel, NULL),
        "close quota runtime kernel");
  check(api.runtime_close(tight_runtime, NULL), "close quota runtime");

  need(full_info.length > 1024 * 1024 &&
           full_info.length + 1024 * 1024 <= MC_HOST_HARD_MAX_SNAPSHOT_TOTAL_BYTES,
       "real snapshot fits aggregate quota test");
  mc_host_runtime_options_v1 aggregate_options = MC_HOST_INIT(mc_host_runtime_options_v1);
  aggregate_options.max_snapshot_total_bytes = (uint32_t)full_info.length + 1024 * 1024;
  mc_host_runtime_t aggregate_runtime = 0;
  check(api.runtime_create(&aggregate_options, &aggregate_runtime, NULL),
        "create runtime with aggregate snapshot quota");
  mc_host_handle_t aggregate_kernel = 0;
  mc_host_kernel_info_v1 aggregate_kernel_info = MC_HOST_INIT(mc_host_kernel_info_v1);
  check(vm_api.kernel_load(aggregate_runtime, (mc_host_bytes_v1){kernel, kernel_len},
                           &aggregate_kernel, &aggregate_kernel_info, NULL),
        "load kernel into aggregate quota runtime");
  mc_host_handle_t retained_baseline = 0;
  mc_host_snapshot_info_v1 retained_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api.baseline_import(aggregate_runtime, aggregate_kernel,
                                 (mc_host_bytes_v1){full, full_info.length},
                                 &retained_baseline, &retained_info, NULL),
        "retain one real snapshot within aggregate quota");
  mc_host_handle_t second_baseline = 0;
  need(snap_api.baseline_import(aggregate_runtime, aggregate_kernel,
                                (mc_host_bytes_v1){full, full_info.length},
                                &second_baseline, &retained_info, NULL) == MC_HOST_RESOURCE_LIMIT &&
           second_baseline == 0,
       "aggregate quota rejects second real baseline");
  mc_host_vm_restore_options_v1 aggregate_restore = MC_HOST_INIT(mc_host_vm_restore_options_v1);
  aggregate_restore.snapshot = (mc_host_bytes_v1){full, full_info.length};
  aggregate_restore.mode = MC_HOST_RESTORE_DETACHED;
  mc_host_handle_t aggregate_vm = 0;
  need(vm_api.vm_restore(aggregate_runtime, aggregate_kernel, 0, &aggregate_restore,
                         &aggregate_vm, NULL) == MC_HOST_RESOURCE_LIMIT && aggregate_vm == 0,
       "aggregate quota rejects restore while baseline is retained");
  mc_host_handle_t capture_vm = 0;
  check(vm_api.vm_create(aggregate_runtime, aggregate_kernel, 0, &create,
                         &capture_vm, NULL), "boot guest under aggregate quota");
  drive_to_prompt(&vm_api, &output_api, aggregate_runtime, capture_vm, NULL);
  mc_host_snapshot_begin_options_v1 aggregate_capture =
      MC_HOST_INIT(mc_host_snapshot_begin_options_v1);
  aggregate_capture.mode = MC_HOST_SNAPSHOT_FULL;
  mc_host_handle_t rejected_snapshot = 0;
  mc_host_snapshot_info_v1 rejected_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  need(snap_api.begin(aggregate_runtime, capture_vm, &aggregate_capture,
                      &rejected_snapshot, &rejected_info, NULL) == MC_HOST_RESOURCE_LIMIT &&
           rejected_snapshot == 0,
       "aggregate quota rejects real capture before allocating a full MCSN");
  mc_host_vm_status_v1 capture_status = MC_HOST_INIT(mc_host_vm_status_v1);
  check(vm_api.vm_status(aggregate_runtime, capture_vm, &capture_status, NULL),
        "failed capture leaves guest available");
  check(snap_api.baseline_close(aggregate_runtime, retained_baseline, NULL),
        "release retained baseline quota");
  mc_host_handle_t recovered_snapshot = 0;
  mc_host_snapshot_info_v1 recovered_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api.begin(aggregate_runtime, capture_vm, &aggregate_capture,
                       &recovered_snapshot, &recovered_info, NULL),
        "capture succeeds after releasing retained quota");
  need(recovered_info.length > 128, "recovered capture is a real MCSN");
  check(snap_api.finish(aggregate_runtime, recovered_snapshot, NULL),
        "release recovered snapshot quota");
  check(vm_api.vm_close(aggregate_runtime, capture_vm, NULL),
        "close guest after quota recovery");
  check(vm_api.kernel_close(aggregate_runtime, aggregate_kernel, NULL),
        "close aggregate quota kernel");
  check(api.runtime_close(aggregate_runtime, NULL), "close aggregate quota runtime");

  mc_host_runtime_options_v1 handle_options = MC_HOST_INIT(mc_host_runtime_options_v1);
  handle_options.max_snapshot_handles = 1;
  mc_host_runtime_t handle_runtime = 0;
  check(api.runtime_create(&handle_options, &handle_runtime, NULL),
        "create one-snapshot-handle runtime");
  mc_host_handle_t handle_kernel = 0;
  mc_host_kernel_info_v1 handle_kernel_info = MC_HOST_INIT(mc_host_kernel_info_v1);
  check(vm_api.kernel_load(handle_runtime, (mc_host_bytes_v1){kernel, kernel_len},
                           &handle_kernel, &handle_kernel_info, NULL),
        "load real kernel into handle quota runtime");
  mc_host_handle_t handle_baseline = 0;
  mc_host_snapshot_info_v1 handle_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  check(snap_api.baseline_import(handle_runtime, handle_kernel,
                                 (mc_host_bytes_v1){full, full_info.length},
                                 &handle_baseline, &handle_info, NULL),
        "retain one real baseline within handle quota");
  mc_host_handle_t refused_baseline = 0;
  need(snap_api.baseline_import(handle_runtime, handle_kernel,
                                (mc_host_bytes_v1){full, full_info.length},
                                &refused_baseline, &handle_info, NULL) == MC_HOST_RESOURCE_LIMIT &&
       refused_baseline == 0, "snapshot handle quota rejects second real baseline");
  mc_host_handle_t handle_vm = 0;
  check(vm_api.vm_create(handle_runtime, handle_kernel, 0, &create, &handle_vm, NULL),
        "boot guest under snapshot handle quota");
  drive_to_prompt(&vm_api, &output_api, handle_runtime, handle_vm, NULL);
  mc_host_handle_t refused_capture = 0;
  mc_host_snapshot_info_v1 handle_capture_info = MC_HOST_INIT(mc_host_snapshot_info_v1);
  need(snap_api.begin(handle_runtime, handle_vm, &aggregate_capture,
                      &refused_capture, &handle_capture_info, NULL) == MC_HOST_RESOURCE_LIMIT &&
       refused_capture == 0, "snapshot handle quota refuses capture before allocation");
  check(snap_api.baseline_close(handle_runtime, handle_baseline, NULL),
        "release baseline handle quota");
  mc_host_handle_t allowed_capture = 0;
  check(snap_api.begin(handle_runtime, handle_vm, &aggregate_capture,
                       &allowed_capture, &handle_capture_info, NULL),
        "capture succeeds after releasing snapshot handle");
  check(snap_api.finish(handle_runtime, allowed_capture, NULL),
        "release allowed snapshot handle");
  check(vm_api.vm_close(handle_runtime, handle_vm, NULL),
        "close snapshot handle quota guest");
  check(vm_api.kernel_close(handle_runtime, handle_kernel, NULL),
        "close snapshot handle quota kernel");
  check(api.runtime_close(handle_runtime, NULL),
        "close snapshot handle quota runtime");

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
  mc_host_buffer_v1 manifest_probe = {NULL, 0, 0};
  need(snap_api.attachment_manifest(runtime, delta_handle, &manifest_probe, NULL) ==
           MC_HOST_BUFFER_TOO_SMALL && manifest_probe.required < 4096,
       "probe companion attachment manifest");
  uint8_t manifest[4096];
  mc_host_buffer_v1 manifest_buffer = {manifest, sizeof(manifest), 0};
  check(snap_api.attachment_manifest(runtime, delta_handle, &manifest_buffer, NULL),
        "read companion attachment manifest");
  need(manifest_buffer.required == manifest_probe.required,
       "manifest read matches probe");
  mc_host_wire_snapshot_attachment_manifest manifest_doc;
  need(mc_host_wire_decode_snapshot_attachment_manifest(manifest,
           manifest_buffer.required, &manifest_doc) == MC_HOST_WIRE_OK &&
           manifest_doc.content_digest.len == 32 &&
           memcmp(manifest_doc.content_digest.data, delta_info.content_digest, 32) == 0 &&
           manifest_doc.routes.count == 2,
       "manifest binds exact MCSN and host routes");
  check(snap_api.finish(runtime, delta_handle, NULL), "finish incremental snapshot read");

  mc_host_vm_restore_options_v1 restore = MC_HOST_INIT(mc_host_vm_restore_options_v1);
  restore.snapshot = (mc_host_bytes_v1){delta, delta_info.length};
  restore.baseline = (mc_host_bytes_v1){full, full_info.length};
  restore.mode = MC_HOST_RESTORE_STRICT;
  restore.flags = MC_HOST_VM_RESTORE_DETERMINISTIC;
  mc_host_handle_t restored = 0;
  mc_host_bytes_v1 manifest_bytes = {manifest, manifest_buffer.required};
  need(vm_api.vm_restore(runtime, kernel_handle, plan, &restore,
                         &restored, NULL) == MC_HOST_INVALID_STATE && restored == 0,
       "strict restore requires companion identity for attached providers");
  need(vm_api.vm_restore_attached(runtime, kernel_handle, 0, &restore,
                                  manifest_bytes, &restored, NULL) ==
           MC_HOST_INVALID_STATE && restored == 0,
       "strict restore rejects missing catalog handler");
  mc_host_handle_t wrong_plan = 0;
  check(attachment_api.plan_create(runtime, &wrong_plan, NULL),
        "create mismatched restore plan");
  check(attachment_api.plan_add(runtime, wrong_plan,
                                (mc_host_bytes_v1){tool_definition.bytes,
                                                   tool_definition.length}, NULL),
        "attach handler to mismatched plan");
  check(attachment_api.plan_add(runtime, wrong_plan,
                                (mc_host_bytes_v1){definition.bytes, definition.length}, NULL),
        "attach mount driver to mismatched plan");
  host_tool_spec.description = (mc_host_wire_bytes){
      (const uint8_t *)"Changed definition", strlen("Changed definition")};
  need(mc_host_wire_encode_host_tool_attachment(&host_tool_spec, host_tool.bytes,
           sizeof(host_tool.bytes), &host_tool.length) == MC_HOST_WIRE_OK,
       "encode mismatched host tool definition");
  host_tool_entry.payload = (mc_host_wire_bytes){host_tool.bytes, host_tool.length};
  need(mc_host_wire_encode_attachment_definition(&host_tool_entry,
           host_tool_definition.bytes, sizeof(host_tool_definition.bytes),
           &host_tool_definition.length) == MC_HOST_WIRE_OK,
       "encode mismatched host tool attachment");
  check(attachment_api.plan_add(runtime, wrong_plan,
                                (mc_host_bytes_v1){host_tool_definition.bytes,
                                                   host_tool_definition.length}, NULL),
        "attach mismatched host tool");
  check(attachment_api.plan_seal(runtime, wrong_plan, NULL),
        "seal mismatched restore plan");
  need(vm_api.vm_restore_attached(runtime, kernel_handle, wrong_plan, &restore,
                         manifest_bytes,
                         &restored, NULL) == MC_HOST_INVALID_STATE && restored == 0,
       "strict restore rejects changed host tool definition");
  check(attachment_api.plan_close(runtime, wrong_plan, NULL),
        "close mismatched restore plan");
  mc_host_handle_t missing_mount_plan = 0;
  check(attachment_api.plan_create(runtime, &missing_mount_plan, NULL),
        "create restore plan without mount driver");
  check(attachment_api.plan_add(runtime, missing_mount_plan,
                                (mc_host_bytes_v1){tool_definition.bytes,
                                                   tool_definition.length}, NULL),
        "attach host tool handler without mount driver");
  host_tool_spec.description = (mc_host_wire_bytes){
      (const uint8_t *)"Native echo 1", strlen("Native echo 1")};
  need(mc_host_wire_encode_host_tool_attachment(&host_tool_spec, host_tool.bytes,
           sizeof(host_tool.bytes), &host_tool.length) == MC_HOST_WIRE_OK,
       "encode matching host tool for mount check");
  host_tool_entry.payload = (mc_host_wire_bytes){host_tool.bytes, host_tool.length};
  need(mc_host_wire_encode_attachment_definition(&host_tool_entry,
           host_tool_definition.bytes, sizeof(host_tool_definition.bytes),
           &host_tool_definition.length) == MC_HOST_WIRE_OK,
       "encode matching host tool attachment for mount check");
  check(attachment_api.plan_add(runtime, missing_mount_plan,
                                (mc_host_bytes_v1){host_tool_definition.bytes,
                                                   host_tool_definition.length}, NULL),
        "attach matching host tool without mount driver");
  check(attachment_api.plan_seal(runtime, missing_mount_plan, NULL),
        "seal plan without mount driver");
  need(vm_api.vm_restore_attached(runtime, kernel_handle, missing_mount_plan, &restore,
                         manifest_bytes,
                         &restored, NULL) == MC_HOST_INVALID_STATE && restored == 0,
       "strict restore rejects missing mounted driver");
  check(attachment_api.plan_close(runtime, missing_mount_plan, NULL),
        "close plan without mount driver");
  mc_host_handle_t wrong_identity_plan = 0;
  check(attachment_api.plan_create(runtime, &wrong_identity_plan, NULL),
        "create restore plan with wrong provider identity");
  wire changed_route = {0}, changed_route_definition = {0};
  put_u16(&changed_route, 29);
  put_u8(&changed_route, 1);
  put_bytes(&changed_route, (const uint8_t *)"/mnt/capi", strlen("/mnt/capi"));
  put_bytes(&changed_route, (const uint8_t *)"different-driver",
            strlen("different-driver"));
  put_u16(&changed_route_definition, 20);
  put_u8(&changed_route_definition, 1);
  put_u32(&changed_route_definition, MC_HOST_ATTACHMENT_HOST_CALL);
  put_bytes(&changed_route_definition, changed_route.bytes, changed_route.length);
  check(attachment_api.plan_add(runtime, wrong_identity_plan,
                                (mc_host_bytes_v1){changed_route_definition.bytes,
                                                   changed_route_definition.length}, NULL),
        "attach same mount name with different provider identity");
  check(attachment_api.plan_add(runtime, wrong_identity_plan,
                                (mc_host_bytes_v1){tool_definition.bytes,
                                                   tool_definition.length}, NULL),
        "attach matching host tool handler to identity plan");
  check(attachment_api.plan_add(runtime, wrong_identity_plan,
                                (mc_host_bytes_v1){host_tool_definition.bytes,
                                                   host_tool_definition.length}, NULL),
        "attach matching host tool definition to identity plan");
  check(attachment_api.plan_add(runtime, wrong_identity_plan,
                                (mc_host_bytes_v1){persist_definition.bytes,
                                                   persist_definition.length}, NULL),
        "attach matching persistence identity");
  check(attachment_api.plan_add(runtime, wrong_identity_plan,
                                (mc_host_bytes_v1){http_definition.bytes,
                                                   http_definition.length}, NULL),
        "attach matching HTTP identity");
  check(attachment_api.plan_seal(runtime, wrong_identity_plan, NULL),
        "seal plan with wrong provider identity");
  need(vm_api.vm_restore_attached(runtime, kernel_handle, wrong_identity_plan,
                                  &restore, manifest_bytes, &restored, NULL) ==
           MC_HOST_INVALID_STATE && restored == 0,
       "strict restore rejects changed provider identity");
  check(attachment_api.plan_close(runtime, wrong_identity_plan, NULL),
        "close plan with wrong provider identity");
  manifest[7] ^= 1;
  need(vm_api.vm_restore_attached(runtime, kernel_handle, plan, &restore,
                                  manifest_bytes, &restored, NULL) ==
           MC_HOST_SNAPSHOT_ERROR && restored == 0,
       "strict restore rejects manifest bound to wrong MCSN");
  manifest[7] ^= 1;
  check(vm_api.vm_restore_attached(runtime, kernel_handle, plan, &restore,
                                  manifest_bytes, &restored, NULL),
        "strict restore real incremental snapshot with matching attachments");
  mc_host_buffer_v1 empty_transcript = {0};
  need(effect_api.transcript_next(runtime, restored, &empty_transcript, NULL) ==
           MC_HOST_WOULD_BLOCK,
       "deterministic restore starts with a fresh effect transcript");
  mc_host_handle_t over_limit_vm = 0;
  need(vm_api.vm_restore(runtime, kernel_handle, 0, &restore, &over_limit_vm, NULL) ==
           MC_HOST_RESOURCE_LIMIT && over_limit_vm == 0,
       "runtime VM limit prevents a third live VM");
  const uint8_t restored_command[] = "git --version\n";
  check(vm_api.terminal_input(runtime, restored,
                              (mc_host_bytes_v1){restored_command, sizeof(restored_command) - 1}, NULL),
        "restored terminal input");
  drive_to_prompt(&vm_api, &output_api, runtime, restored, "agentos-git");

  cancel_vm_with_operation(&vm_api, &op_api, runtime, restored);
  mc_host_handle_t forked = 0;
  check(vm_api.vm_fork(runtime, vm, &forked, NULL),
        "fork quiescent VM through full snapshot and strict restore");
  need(forked != 0 && forked != vm, "fork has independent VM identity");
  const uint8_t fork_command[] = "echo fork-child\n";
  check(vm_api.terminal_input(runtime, forked,
                              (mc_host_bytes_v1){fork_command, sizeof(fork_command) - 1}, NULL),
        "send terminal input to fork child");
  drive_to_prompt(&vm_api, &output_api, runtime, forked, "fork-child");
  status_close_probe close_probe = {&vm_api, runtime, forked, 0, 0};
  pthread_t observer;
  need(pthread_create(&observer, NULL, observe_until_closed, &close_probe) == 0,
       "start concurrent VM observer");
  while (!atomic_load_explicit(&close_probe.observed, memory_order_acquire)) sched_yield();
  check(vm_api.vm_close(runtime, forked, NULL), "close fork child during status reads");
  need(pthread_join(observer, NULL) == 0 &&
           atomic_load_explicit(&close_probe.stale, memory_order_acquire),
       "retire VM handle across concurrent status calls");
  wire snapshot_unmount = mount_request(MC_HOST_OP_UNMOUNT, "/mnt/capi");
  mc_host_handle_t snapshot_unmount_op = 0;
  check(op_api.start(runtime, vm,
                     (mc_host_bytes_v1){snapshot_unmount.bytes, snapshot_unmount.length},
                     &snapshot_unmount_op, NULL), "unmount snapshot driver from source");
  check(op_api.close(runtime, snapshot_unmount_op, NULL),
        "close snapshot unmount operation");
  run_effect_pressure(&vm_api, &op_api, &effect_api, runtime, vm);
  run_min_stack_probe(&vm_api, &op_api, runtime, vm);
  cancel_mount_relay(&vm_api, &op_api, &effect_api, runtime, vm);
  run_native_connection(&vm_api, &op_api, &effect_api, &snap_api,
                        &output_api, &attachment_api, runtime, kernel_handle, &create);
  size_t rust_full_length = 0, rust_bundle_length = 0;
  uint8_t *rust_full = read_artifact(argv[5], &rust_full_length);
  uint8_t *rust_bundle = read_artifact(argv[6], &rust_bundle_length);
  need(rust_bundle_length > 4, "Rust-host incremental bundle");
  uint32_t rust_base_length = (uint32_t)rust_bundle[0] |
      (uint32_t)rust_bundle[1] << 8 | (uint32_t)rust_bundle[2] << 16 |
      (uint32_t)rust_bundle[3] << 24;
  need(rust_base_length > 128 && rust_base_length < rust_bundle_length - 4,
       "bounded Rust-host baseline in incremental bundle");
  verify_rust_host_snapshot(&vm_api, &op_api, &snap_api, runtime, kernel_handle,
                            rust_full, rust_full_length, NULL, 0);
  size_t base_image_length = 0;
  uint8_t *base_image = read_artifact(argv[4], &base_image_length);
  verify_identical_capture(&vm_api, &op_api, &snap_api, runtime, kernel_handle,
                           base_image, base_image_length, rust_full, rust_full_length);
  verify_operation_result_quota(&api, &vm_api, &op_api, &output_api,
                                kernel, kernel_len, base_image, base_image_length);
  free(base_image);
  verify_rust_host_snapshot(&vm_api, &op_api, &snap_api, runtime, kernel_handle,
                            rust_bundle + 4 + rust_base_length,
                            rust_bundle_length - 4 - rust_base_length,
                            rust_bundle + 4, rust_base_length);
  mc_host_vm_restore_options_v1 missing_base = MC_HOST_INIT(mc_host_vm_restore_options_v1);
  missing_base.snapshot = (mc_host_bytes_v1){rust_bundle + 4 + rust_base_length,
      rust_bundle_length - 4 - rust_base_length};
  missing_base.mode = MC_HOST_RESTORE_DETACHED;
  mc_host_handle_t rejected_vm = 0;
  need(vm_api.vm_restore(runtime, kernel_handle, 0, &missing_base,
                         &rejected_vm, NULL) == MC_HOST_SNAPSHOT_ERROR && rejected_vm == 0,
       "incremental restore requires its full baseline");
  rust_bundle[4 + rust_base_length - 1] ^= 1;
  missing_base.baseline = (mc_host_bytes_v1){rust_bundle + 4, rust_base_length};
  need(vm_api.vm_restore(runtime, kernel_handle, 0, &missing_base,
                         &rejected_vm, NULL) == MC_HOST_SNAPSHOT_ERROR && rejected_vm == 0,
       "corrupt incremental baseline cannot publish a VM");
  rust_full[32] ^= 1; /* MCSN kernel digest offset from snapshot.kdl. */
  mc_host_vm_restore_options_v1 wrong_kernel = MC_HOST_INIT(mc_host_vm_restore_options_v1);
  wrong_kernel.snapshot = (mc_host_bytes_v1){rust_full, rust_full_length};
  wrong_kernel.mode = MC_HOST_RESTORE_DETACHED;
  need(vm_api.vm_restore(runtime, kernel_handle, 0, &wrong_kernel,
                         &rejected_vm, NULL) == MC_HOST_SNAPSHOT_ERROR && rejected_vm == 0,
       "kernel digest mismatch cannot publish a VM");
  free(rust_bundle);
  free(rust_full);
  check(snap_api.baseline_close(runtime, baseline, NULL), "close baseline");
  mc_host_handle_t live_plan = 0, live_vm = 0, live_child = 0;
  check(attachment_api.plan_create(runtime, &live_plan, NULL),
        "create plan for live catalog fork");
  check(attachment_api.plan_add(runtime, live_plan,
                                (mc_host_bytes_v1){tool_definition.bytes,
                                                   tool_definition.length}, NULL),
        "register live catalog route without create-time tool definition");
  check(attachment_api.plan_seal(runtime, live_plan, NULL),
        "seal live catalog fork plan");
  check(vm_api.vm_create(runtime, kernel_handle, live_plan, &create,
                         &live_vm, NULL), "boot VM for live catalog fork");
  drive_to_prompt(&vm_api, &output_api, runtime, live_vm, NULL);
  mc_host_handle_t expanded_plan = 0;
  check(attachment_api.plan_create(runtime, &expanded_plan, NULL),
        "create replacement live attachment plan");
  check(attachment_api.plan_add(runtime, expanded_plan,
        (mc_host_bytes_v1){tool_definition.bytes, tool_definition.length}, NULL),
        "retain native echo route in replacement plan");
  wire added_route = {0}, added_definition = {0};
  put_u16(&added_route, 29);
  put_u8(&added_route, 1);
  put_bytes(&added_route, (const uint8_t *)"/mnt/live", 9);
  put_bytes(&added_route, (const uint8_t *)"live-mount-driver", 17);
  put_u16(&added_definition, 20);
  put_u8(&added_definition, 1);
  put_u32(&added_definition, MC_HOST_ATTACHMENT_HOST_CALL);
  put_bytes(&added_definition, added_route.bytes, added_route.length);
  check(attachment_api.plan_add(runtime, expanded_plan,
        (mc_host_bytes_v1){added_definition.bytes, added_definition.length}, NULL),
        "add live mount route");
  check(attachment_api.plan_seal(runtime, expanded_plan, NULL),
        "seal replacement attachment plan");
  check(attachment_api.vm_attach_update(runtime, live_vm, expanded_plan, NULL),
        "apply live host-call route replacement");
  uint8_t live_manifest_data[512];
  mc_host_buffer_v1 live_manifest_buffer = {live_manifest_data, sizeof(live_manifest_data), 0};
  check(attachment_api.vm_attachment_manifest(runtime, live_vm, &live_manifest_buffer, NULL),
        "read live attachment manifest");
  mc_host_wire_live_attachment_manifest live_manifest;
  need(mc_host_wire_decode_live_attachment_manifest(live_manifest_data,
        live_manifest_buffer.required, &live_manifest) == MC_HOST_WIRE_OK &&
       live_manifest.routes.count == 2,
       "live manifest includes both registered routes");
  wire live_mount = mount_request(MC_HOST_OP_MOUNT, "/mnt/live");
  mc_host_handle_t live_mount_op = 0;
  check(op_api.start(runtime, live_vm,
        (mc_host_bytes_v1){live_mount.bytes, live_mount.length}, &live_mount_op, NULL),
        "mount newly attached route");
  check(op_api.close(runtime, live_mount_op, NULL), "close live mount operation");
  need(attachment_api.vm_attach_update(runtime, live_vm, 0, NULL) == MC_HOST_INVALID_STATE,
       "live update cannot remove mounted driver");
  wire live_unmount = mount_request(MC_HOST_OP_UNMOUNT, "/mnt/live");
  check(op_api.start(runtime, live_vm,
        (mc_host_bytes_v1){live_unmount.bytes, live_unmount.length}, &live_mount_op, NULL),
        "unmount live driver");
  check(op_api.close(runtime, live_mount_op, NULL), "close live unmount operation");
  run_catalog_apply(&vm_api, &op_api, runtime, live_vm);
  need(attachment_api.vm_attach_update(runtime, live_vm, 0, NULL) == MC_HOST_INVALID_STATE,
       "live update cannot remove catalog handler");
  check(vm_api.vm_fork(runtime, live_vm, &live_child, NULL),
        "fork VM with catalog added after creation");
  need(live_child != 0 && live_child != live_vm,
       "live catalog fork publishes independent child");
  check(vm_api.vm_close(runtime, live_child, NULL), "close live catalog fork child");
  check(vm_api.vm_close(runtime, live_vm, NULL), "close live catalog fork source");
  check(attachment_api.plan_close(runtime, live_plan, NULL),
        "close live catalog fork plan");
  check(attachment_api.plan_close(runtime, expanded_plan, NULL),
        "close replacement attachment plan");
  check(vm_api.kernel_close(runtime, kernel_handle, NULL), "close kernel");
  check(attachment_api.plan_close(runtime, plan, NULL), "close attachment plan");
  need(atomic_load_explicit(&wake.entered, memory_order_acquire) != 0,
       "real guest output and effects delivered wake hints");
  need(api.runtime_close(runtime, NULL) == MC_HOST_BUSY,
       "runtime close waits for application wake callback");
  atomic_store_explicit(&wake.release, 1, memory_order_release);
  mc_host_result_t close_result = MC_HOST_BUSY;
  for (int attempt = 0; attempt < 100000 && close_result == MC_HOST_BUSY; ++attempt) {
    close_result = api.runtime_close(runtime, NULL);
    if (close_result == MC_HOST_BUSY) sched_yield();
  }
  check(close_result, "close runtime after wake delivery");
  need(atomic_load_explicit(&wake.calls, memory_order_relaxed) > 0,
       "wake callback ran on the real VM path");
  free(delta);
  free(full);
  free(image);
  free(kernel);
  dlclose(lib);
  return 0;
}
