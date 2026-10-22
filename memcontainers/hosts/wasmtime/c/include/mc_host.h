#ifndef MC_HOST_H
#define MC_HOST_H

#include <stdint.h>
#include "memcontainers/contracts/ctl.gen.h"
#include "memcontainers/contracts/embedder.gen.h"

/* Linux callers use at least MC_HOST_MIN_NATIVE_STACK_BYTES for threads that
 * load kernels, create or restore VMs, drive, or call trusted controls. The
 * Wasmtime engine handles SIGSEGV, SIGBUS, SIGILL, and SIGFPE for guest traps;
 * application handlers installed later must chain to the prior handlers. */

#if defined(_WIN32)
#  if defined(MC_HOST_BUILD)
#    define MC_HOST_EXPORT __declspec(dllexport)
#  else
#    define MC_HOST_EXPORT __declspec(dllimport)
#  endif
#  define MC_HOST_CALL __cdecl
#else
#  define MC_HOST_EXPORT __attribute__((visibility("default")))
#  define MC_HOST_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t mc_host_runtime_t;
typedef uint64_t mc_host_handle_t;
typedef uint32_t mc_host_result_t;

typedef struct mc_host_header_v1 {
  uint32_t size;
  uint16_t version;
  uint16_t flags;
} mc_host_header_v1;

typedef struct mc_host_buffer_v1 {
  uint8_t *data;
  uint64_t capacity;
  uint64_t required;
} mc_host_buffer_v1;

typedef struct mc_host_bytes_v1 {
  const uint8_t *data;
  uint64_t len;
} mc_host_bytes_v1;

/* Wake is an edge hint on one library thread. It may coalesce notifications.
 * The callback must not call this API. Keep user_data valid until runtime_close.
 * Drain status, output, and effects until WOULD_BLOCK. */
typedef void (MC_HOST_CALL *mc_host_wake_fn)(void *user_data);

typedef struct mc_host_error_v1 {
  mc_host_header_v1 header;
  uint32_t domain;
  int32_t code;
  uint8_t retryable;
  uint8_t reserved[7];
  mc_host_buffer_v1 message;
  /* Zero when no operation is associated with the error. */
  uint64_t operation_id;
} mc_host_error_v1;

typedef struct mc_host_runtime_options_v1 {
  mc_host_header_v1 header;
  /* Zero selects the published default. Nonzero values may lower the limit. */
  uint32_t max_vms;
  uint32_t max_drive_ticks;
  /* Zero selects the default. At least 4 bytes and 4 records otherwise. */
  uint32_t max_output_bytes;
  uint32_t max_output_records;
  /* Maximum size of one MCSN file. Zero selects the default. */
  uint32_t max_snapshot_bytes;
  /* Aggregate retained snapshots, baselines, and restore buffers. */
  uint32_t max_snapshot_total_bytes;
  /* Maximum outstanding control operations on one VM. */
  uint32_t max_operations_per_vm;
  uint32_t max_effect_events;
  uint32_t max_effect_bytes;
  uint32_t max_effect_response_bytes;
  uint32_t max_ws_sockets;
  uint32_t max_ws_incoming_bytes;
  mc_host_wake_fn wake;
  void *wake_user_data;
  uint32_t max_kernels;
  uint32_t max_kernel_bytes;
  uint32_t max_compiled_modules;
  uint32_t max_image_bytes;
  uint32_t max_snapshot_handles;
  uint32_t reserved_resource_options;
  /* Maximum encoded result of one control operation. */
  uint32_t max_operation_result_bytes;
  uint32_t reserved_result_options;
  /* Maximum encoded native message frame. Zero selects the default. */
  uint32_t max_message_bytes;
  uint32_t reserved_message_options;
  /* Per-event and per-response limits, separate from aggregate queue limits. */
  uint32_t max_effect_event_bytes;
  uint32_t max_effect_single_response_bytes;
} mc_host_runtime_options_v1;

typedef struct mc_host_library_info_v1 {
  mc_host_header_v1 header;
  uint16_t abi_major;
  uint16_t abi_minor;
  uint32_t implementation_table_size;
  uint64_t feature_bits;
} mc_host_library_info_v1;

typedef struct mc_host_api_v1 {
  mc_host_header_v1 header;
  uint16_t abi_major;
  uint16_t abi_minor;
  uint32_t implementation_table_size;
  uint64_t feature_bits;
  mc_host_result_t (MC_HOST_CALL *runtime_create)(
      const mc_host_runtime_options_v1 *, mc_host_runtime_t *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *runtime_close)(
      mc_host_runtime_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *library_info)(
      mc_host_library_info_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *get_interface)(
      mc_host_runtime_t, uint32_t, uint16_t, uint32_t, void *, mc_host_error_v1 *);
} mc_host_api_v1;

typedef struct mc_host_kernel_info_v1 {
  mc_host_header_v1 header;
  uint8_t digest[32];
} mc_host_kernel_info_v1;

typedef struct mc_host_vm_create_options_v1 {
  mc_host_header_v1 header;
  const mc_host_bytes_v1 *layers;
  uint64_t layer_count;
  int32_t contract_tier;
  int32_t contract_budget_mib;
  int64_t contract_fuel;
  int32_t requested_workers; /* -1 uses the host default. */
  /* DETERMINISTIC records application effects; native network is unsupported. */
  uint32_t flags;
  uint32_t terminal_cols;
  uint32_t terminal_rows;
} mc_host_vm_create_options_v1;

typedef struct mc_host_vm_restore_options_v1 {
  mc_host_header_v1 header;
  mc_host_bytes_v1 snapshot;
  mc_host_bytes_v1 baseline;
  uint32_t mode;
  uint32_t flags; /* DETERMINISTIC requires application-managed effects. */
} mc_host_vm_restore_options_v1;

typedef struct mc_host_drive_options_v1 {
  mc_host_header_v1 header;
  uint32_t max_ticks;
  uint32_t reserved;
  uint64_t stop_mask;
} mc_host_drive_options_v1;

typedef struct mc_host_vm_status_v1 {
  mc_host_header_v1 header;
  uint32_t state;
  uint32_t last_tick;
  uint32_t flags;
  int32_t exit_code;
  uint64_t output_records;
  uint64_t output_bytes;
  uint64_t output_dropped_bytes;
  int32_t inflight_egress;
  int32_t pending_commits;
  uint32_t workers;
  /* Effect requests that still await an application answer. */
  uint32_t active_effects;
} mc_host_vm_status_v1;

typedef struct mc_host_drive_result_v1 {
  mc_host_header_v1 header;
  uint32_t ticks;
  uint32_t stop_reason;
  mc_host_vm_status_v1 status;
} mc_host_drive_result_v1;

typedef struct mc_host_vm_api_v1 {
  mc_host_header_v1 header;
  mc_host_result_t (MC_HOST_CALL *kernel_load)(
      mc_host_runtime_t, mc_host_bytes_v1, mc_host_handle_t *,
      mc_host_kernel_info_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *kernel_close)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *vm_create)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_handle_t,
      const mc_host_vm_create_options_v1 *, mc_host_handle_t *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *vm_restore)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_handle_t,
      const mc_host_vm_restore_options_v1 *, mc_host_handle_t *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *vm_status)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_vm_status_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *vm_drive)(
      mc_host_runtime_t, mc_host_handle_t, const mc_host_drive_options_v1 *,
      mc_host_drive_result_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *vm_close)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *vm_cancel_and_close)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *terminal_input)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_bytes_v1, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *terminal_resize)(
      mc_host_runtime_t, mc_host_handle_t, uint32_t, uint32_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *vm_restore_attached)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_handle_t,
      const mc_host_vm_restore_options_v1 *, mc_host_bytes_v1,
      mc_host_handle_t *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *vm_fork)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_handle_t *, mc_host_error_v1 *);
} mc_host_vm_api_v1;

typedef struct mc_host_output_record_v1 {
  mc_host_header_v1 header;
  uint32_t kind;
  uint32_t channel;
  uint64_t sequence;
  uint64_t byte_offset;
  uint64_t length;
  uint64_t first_lost_sequence;
  uint64_t last_lost_sequence;
  uint64_t byte_end;
} mc_host_output_record_v1;

typedef struct mc_host_output_stats_v1 {
  mc_host_header_v1 header;
  uint64_t records;
  uint64_t retained_bytes;
  uint64_t dropped_bytes;
  uint64_t leased_records;
  uint32_t flags;
  uint32_t reserved;
} mc_host_output_stats_v1;

typedef struct mc_host_output_api_v1 {
  mc_host_header_v1 header;
  mc_host_result_t (MC_HOST_CALL *next)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_handle_t *,
      mc_host_output_record_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *read)(
      mc_host_runtime_t, mc_host_handle_t, uint64_t,
      mc_host_buffer_v1 *, uint64_t *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *ack)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *release)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *stats)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_output_stats_v1 *, mc_host_error_v1 *);
} mc_host_output_api_v1;

typedef struct mc_host_snapshot_begin_options_v1 {
  mc_host_header_v1 header;
  uint32_t mode;
  uint32_t reserved;
  mc_host_handle_t baseline;
} mc_host_snapshot_begin_options_v1;

typedef struct mc_host_snapshot_info_v1 {
  mc_host_header_v1 header;
  uint32_t kind;
  uint32_t mcsn_version;
  uint8_t kernel_digest[32];
  uint8_t baseline_id[32];
  uint8_t content_digest[32];
  uint64_t length;
  uint64_t memory_len;
} mc_host_snapshot_info_v1;

typedef struct mc_host_snapshot_readiness_v1 {
  mc_host_header_v1 header;
  uint32_t inflight_egress;
  uint32_t pending_commits;
  uint32_t pending_operations;
  uint32_t pending_effects;
  uint32_t pending_transactions;
  uint32_t reserved;
} mc_host_snapshot_readiness_v1;

typedef struct mc_host_snapshot_api_v1 {
  mc_host_header_v1 header;
  mc_host_result_t (MC_HOST_CALL *baseline_import)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_bytes_v1,
      mc_host_handle_t *, mc_host_snapshot_info_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *baseline_close)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *begin)(
      mc_host_runtime_t, mc_host_handle_t,
      const mc_host_snapshot_begin_options_v1 *, mc_host_handle_t *,
      mc_host_snapshot_info_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *read)(
      mc_host_runtime_t, mc_host_handle_t, uint64_t,
      mc_host_buffer_v1 *, uint64_t *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *finish)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *cancel)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *inspect)(
      mc_host_runtime_t, mc_host_bytes_v1, mc_host_bytes_v1,
      mc_host_snapshot_info_v1 *, mc_host_error_v1 *);
  /* Advisory. begin rechecks quiescence in the VM store. */
  mc_host_result_t (MC_HOST_CALL *readiness)(
      mc_host_runtime_t, mc_host_handle_t,
      mc_host_snapshot_readiness_v1 *, mc_host_error_v1 *);
  /* Store this companion with the MCSN and authenticate both before restore. */
  mc_host_result_t (MC_HOST_CALL *attachment_manifest)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_buffer_v1 *, mc_host_error_v1 *);
} mc_host_snapshot_api_v1;

typedef struct mc_host_operation_status_v1 {
  mc_host_header_v1 header;
  uint32_t kind;
  uint32_t state;
  uint32_t error;
  uint32_t reserved;
  uint64_t result_length;
} mc_host_operation_status_v1;

typedef struct mc_host_operation_api_v1 {
  mc_host_header_v1 header;
  mc_host_result_t (MC_HOST_CALL *start)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_bytes_v1,
      mc_host_handle_t *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *poll)(
      mc_host_runtime_t, mc_host_handle_t,
      mc_host_operation_status_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *result)(
      mc_host_runtime_t, mc_host_handle_t,
      mc_host_buffer_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *peek)(
      mc_host_runtime_t, mc_host_handle_t, uint32_t, uint64_t,
      mc_host_buffer_v1 *, uint64_t *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *cancel)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *close)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
} mc_host_operation_api_v1;

typedef struct mc_host_attachment_api_v1 {
  mc_host_header_v1 header;
  mc_host_result_t (MC_HOST_CALL *plan_create)(
      mc_host_runtime_t, mc_host_handle_t *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *plan_add)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_bytes_v1, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *plan_seal)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *plan_close)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  /* Atomically replace host-call routes and host-tool definitions in a quiescent VM. */
  mc_host_result_t (MC_HOST_CALL *vm_attach_update)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *vm_attachment_manifest)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_buffer_v1 *, mc_host_error_v1 *);
} mc_host_attachment_api_v1;

typedef struct mc_host_effect_info_v1 {
  mc_host_header_v1 header;
  uint32_t kind;
  uint32_t reserved;
  uint64_t length;
} mc_host_effect_info_v1;

typedef struct mc_host_effect_stats_v1 {
  mc_host_header_v1 header;
  uint64_t queued_events;
  uint64_t leased_events;
  uint64_t queued_bytes;
  uint64_t response_bytes;
  uint64_t refused_host_call;
  uint64_t refused_persist;
  uint64_t refused_http;
  uint64_t refused_ws;
  uint64_t refused_approval;
} mc_host_effect_stats_v1;

typedef struct mc_host_effect_api_v1 {
  mc_host_header_v1 header;
  mc_host_result_t (MC_HOST_CALL *next)(
      mc_host_runtime_t, mc_host_handle_t, uint64_t,
      mc_host_handle_t *, mc_host_effect_info_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *read)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_buffer_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *release)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *respond)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_bytes_v1, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *ack)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *response_begin)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_bytes_v1,
      uint64_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *response_write)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_bytes_v1, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *response_finish)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *response_abort)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *socket_push)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_bytes_v1, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *socket_close)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *info)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_effect_info_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *cancel)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *stats)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_effect_stats_v1 *, mc_host_error_v1 *);
  /* Deterministic VMs retain ordered external inputs until acknowledged.
   * The byte limit is max_effect_bytes + max_effect_response_bytes. A full
   * transcript refuses further effects until the caller drains it. Records
   * can contain application data. BUFFER_TOO_SMALL does not consume a record. */
  mc_host_result_t (MC_HOST_CALL *transcript_next)(
      mc_host_runtime_t, mc_host_handle_t, mc_host_buffer_v1 *, mc_host_error_v1 *);
  mc_host_result_t (MC_HOST_CALL *transcript_ack)(
      mc_host_runtime_t, mc_host_handle_t, uint64_t, mc_host_error_v1 *);
} mc_host_effect_api_v1;

#define MC_HOST_INIT(type_) { { (uint32_t)sizeof(type_), 1u, 0u } }

typedef mc_host_result_t (MC_HOST_CALL *mc_host_get_api_fn)(
    uint16_t, uint16_t, uint32_t, void *);

MC_HOST_EXPORT mc_host_result_t MC_HOST_CALL mc_host_get_api(
    uint16_t requested_major, uint16_t minimum_minor,
    uint32_t caller_table_size, void *out_table);

#ifdef __cplusplus
}
#endif
#endif
