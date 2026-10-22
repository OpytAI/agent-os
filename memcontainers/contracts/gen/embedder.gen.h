// @generated from contracts/embedder.kdl by //contracts/codegen:projector — do not edit.
#ifndef MC_EMBEDDER_CONTRACT_H
#define MC_EMBEDDER_CONTRACT_H

#include <stdint.h>

#define MC_HOST_ABI_MAJOR UINT16_C(1)
#define MC_HOST_ABI_MINOR UINT16_C(1)
#define MC_HOST_LAYOUT_HEADER_SIZE 8
#define MC_HOST_LAYOUT_HEADER_ALIGN 4
#define MC_HOST_LAYOUT_HEADER_SIZE_OFFSET 0
#define MC_HOST_LAYOUT_HEADER_VERSION_OFFSET 4
#define MC_HOST_LAYOUT_HEADER_FLAGS_OFFSET 6
#define MC_HOST_LAYOUT_BUFFER_SIZE 24
#define MC_HOST_LAYOUT_BUFFER_ALIGN 8
#define MC_HOST_LAYOUT_BUFFER_DATA_OFFSET 0
#define MC_HOST_LAYOUT_BUFFER_CAPACITY_OFFSET 8
#define MC_HOST_LAYOUT_BUFFER_REQUIRED_OFFSET 16
#define MC_HOST_LAYOUT_ERROR_SIZE 56
#define MC_HOST_LAYOUT_ERROR_ALIGN 8
#define MC_HOST_LAYOUT_ERROR_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_ERROR_DOMAIN_OFFSET 8
#define MC_HOST_LAYOUT_ERROR_CODE_OFFSET 12
#define MC_HOST_LAYOUT_ERROR_RETRYABLE_OFFSET 16
#define MC_HOST_LAYOUT_ERROR_RESERVED_OFFSET 17
#define MC_HOST_LAYOUT_ERROR_MESSAGE_OFFSET 24
#define MC_HOST_LAYOUT_ERROR_OPERATION_ID_OFFSET 48
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_SIZE 120
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_ALIGN 8
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_VMS_OFFSET 8
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_DRIVE_TICKS_OFFSET 12
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_OUTPUT_BYTES_OFFSET 16
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_OUTPUT_RECORDS_OFFSET 20
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_SNAPSHOT_BYTES_OFFSET 24
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_SNAPSHOT_TOTAL_BYTES_OFFSET 28
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_OPERATIONS_PER_VM_OFFSET 32
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_EFFECT_EVENTS_OFFSET 36
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_EFFECT_BYTES_OFFSET 40
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_EFFECT_RESPONSE_BYTES_OFFSET 44
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_WS_SOCKETS_OFFSET 48
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_WS_INCOMING_BYTES_OFFSET 52
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_WAKE_OFFSET 56
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_WAKE_USER_DATA_OFFSET 64
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_KERNELS_OFFSET 72
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_KERNEL_BYTES_OFFSET 76
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_COMPILED_MODULES_OFFSET 80
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_IMAGE_BYTES_OFFSET 84
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_SNAPSHOT_HANDLES_OFFSET 88
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_RESERVED_RESOURCE_OPTIONS_OFFSET 92
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_OPERATION_RESULT_BYTES_OFFSET 96
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_RESERVED_RESULT_OPTIONS_OFFSET 100
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_MESSAGE_BYTES_OFFSET 104
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_RESERVED_MESSAGE_OPTIONS_OFFSET 108
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_EFFECT_EVENT_BYTES_OFFSET 112
#define MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_EFFECT_SINGLE_RESPONSE_BYTES_OFFSET 116
#define MC_HOST_LAYOUT_LIBRARY_INFO_SIZE 24
#define MC_HOST_LAYOUT_LIBRARY_INFO_ALIGN 8
#define MC_HOST_LAYOUT_LIBRARY_INFO_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_LIBRARY_INFO_ABI_MAJOR_OFFSET 8
#define MC_HOST_LAYOUT_LIBRARY_INFO_ABI_MINOR_OFFSET 10
#define MC_HOST_LAYOUT_LIBRARY_INFO_IMPLEMENTATION_TABLE_SIZE_OFFSET 12
#define MC_HOST_LAYOUT_LIBRARY_INFO_FEATURE_BITS_OFFSET 16
#define MC_HOST_LAYOUT_API_SIZE 56
#define MC_HOST_LAYOUT_API_ALIGN 8
#define MC_HOST_LAYOUT_API_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_API_ABI_MAJOR_OFFSET 8
#define MC_HOST_LAYOUT_API_ABI_MINOR_OFFSET 10
#define MC_HOST_LAYOUT_API_IMPLEMENTATION_TABLE_SIZE_OFFSET 12
#define MC_HOST_LAYOUT_API_FEATURE_BITS_OFFSET 16
#define MC_HOST_LAYOUT_API_RUNTIME_CREATE_OFFSET 24
#define MC_HOST_LAYOUT_API_RUNTIME_CLOSE_OFFSET 32
#define MC_HOST_LAYOUT_API_LIBRARY_INFO_OFFSET 40
#define MC_HOST_LAYOUT_API_GET_INTERFACE_OFFSET 48
#define MC_HOST_LAYOUT_BYTES_SIZE 16
#define MC_HOST_LAYOUT_BYTES_ALIGN 8
#define MC_HOST_LAYOUT_BYTES_DATA_OFFSET 0
#define MC_HOST_LAYOUT_BYTES_LEN_OFFSET 8
#define MC_HOST_LAYOUT_KERNEL_INFO_SIZE 40
#define MC_HOST_LAYOUT_KERNEL_INFO_ALIGN 4
#define MC_HOST_LAYOUT_KERNEL_INFO_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_KERNEL_INFO_DIGEST_OFFSET 8
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_SIZE 56
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_ALIGN 8
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_LAYERS_OFFSET 8
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_LAYER_COUNT_OFFSET 16
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_CONTRACT_TIER_OFFSET 24
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_CONTRACT_BUDGET_MIB_OFFSET 28
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_CONTRACT_FUEL_OFFSET 32
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_REQUESTED_WORKERS_OFFSET 40
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_FLAGS_OFFSET 44
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_TERMINAL_COLS_OFFSET 48
#define MC_HOST_LAYOUT_VM_CREATE_OPTIONS_TERMINAL_ROWS_OFFSET 52
#define MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_SIZE 48
#define MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_ALIGN 8
#define MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_SNAPSHOT_OFFSET 8
#define MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_BASELINE_OFFSET 24
#define MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_MODE_OFFSET 40
#define MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_FLAGS_OFFSET 44
#define MC_HOST_LAYOUT_DRIVE_OPTIONS_SIZE 24
#define MC_HOST_LAYOUT_DRIVE_OPTIONS_ALIGN 8
#define MC_HOST_LAYOUT_DRIVE_OPTIONS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_DRIVE_OPTIONS_MAX_TICKS_OFFSET 8
#define MC_HOST_LAYOUT_DRIVE_OPTIONS_RESERVED_OFFSET 12
#define MC_HOST_LAYOUT_DRIVE_OPTIONS_STOP_MASK_OFFSET 16
#define MC_HOST_LAYOUT_VM_STATUS_SIZE 64
#define MC_HOST_LAYOUT_VM_STATUS_ALIGN 8
#define MC_HOST_LAYOUT_VM_STATUS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_VM_STATUS_STATE_OFFSET 8
#define MC_HOST_LAYOUT_VM_STATUS_LAST_TICK_OFFSET 12
#define MC_HOST_LAYOUT_VM_STATUS_FLAGS_OFFSET 16
#define MC_HOST_LAYOUT_VM_STATUS_EXIT_CODE_OFFSET 20
#define MC_HOST_LAYOUT_VM_STATUS_OUTPUT_RECORDS_OFFSET 24
#define MC_HOST_LAYOUT_VM_STATUS_OUTPUT_BYTES_OFFSET 32
#define MC_HOST_LAYOUT_VM_STATUS_OUTPUT_DROPPED_BYTES_OFFSET 40
#define MC_HOST_LAYOUT_VM_STATUS_INFLIGHT_EGRESS_OFFSET 48
#define MC_HOST_LAYOUT_VM_STATUS_PENDING_COMMITS_OFFSET 52
#define MC_HOST_LAYOUT_VM_STATUS_WORKERS_OFFSET 56
#define MC_HOST_LAYOUT_VM_STATUS_ACTIVE_EFFECTS_OFFSET 60
#define MC_HOST_LAYOUT_DRIVE_RESULT_SIZE 80
#define MC_HOST_LAYOUT_DRIVE_RESULT_ALIGN 8
#define MC_HOST_LAYOUT_DRIVE_RESULT_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_DRIVE_RESULT_TICKS_OFFSET 8
#define MC_HOST_LAYOUT_DRIVE_RESULT_STOP_REASON_OFFSET 12
#define MC_HOST_LAYOUT_DRIVE_RESULT_STATUS_OFFSET 16
#define MC_HOST_LAYOUT_VM_API_SIZE 104
#define MC_HOST_LAYOUT_VM_API_ALIGN 8
#define MC_HOST_LAYOUT_VM_API_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_VM_API_KERNEL_LOAD_OFFSET 8
#define MC_HOST_LAYOUT_VM_API_KERNEL_CLOSE_OFFSET 16
#define MC_HOST_LAYOUT_VM_API_VM_CREATE_OFFSET 24
#define MC_HOST_LAYOUT_VM_API_VM_RESTORE_OFFSET 32
#define MC_HOST_LAYOUT_VM_API_VM_STATUS_OFFSET 40
#define MC_HOST_LAYOUT_VM_API_VM_DRIVE_OFFSET 48
#define MC_HOST_LAYOUT_VM_API_VM_CLOSE_OFFSET 56
#define MC_HOST_LAYOUT_VM_API_VM_CANCEL_AND_CLOSE_OFFSET 64
#define MC_HOST_LAYOUT_VM_API_TERMINAL_INPUT_OFFSET 72
#define MC_HOST_LAYOUT_VM_API_TERMINAL_RESIZE_OFFSET 80
#define MC_HOST_LAYOUT_VM_API_VM_RESTORE_ATTACHED_OFFSET 88
#define MC_HOST_LAYOUT_VM_API_VM_FORK_OFFSET 96
#define MC_HOST_LAYOUT_OUTPUT_RECORD_SIZE 64
#define MC_HOST_LAYOUT_OUTPUT_RECORD_ALIGN 8
#define MC_HOST_LAYOUT_OUTPUT_RECORD_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_OUTPUT_RECORD_KIND_OFFSET 8
#define MC_HOST_LAYOUT_OUTPUT_RECORD_CHANNEL_OFFSET 12
#define MC_HOST_LAYOUT_OUTPUT_RECORD_SEQUENCE_OFFSET 16
#define MC_HOST_LAYOUT_OUTPUT_RECORD_BYTE_OFFSET_OFFSET 24
#define MC_HOST_LAYOUT_OUTPUT_RECORD_LENGTH_OFFSET 32
#define MC_HOST_LAYOUT_OUTPUT_RECORD_FIRST_LOST_SEQUENCE_OFFSET 40
#define MC_HOST_LAYOUT_OUTPUT_RECORD_LAST_LOST_SEQUENCE_OFFSET 48
#define MC_HOST_LAYOUT_OUTPUT_RECORD_BYTE_END_OFFSET 56
#define MC_HOST_LAYOUT_OUTPUT_STATS_SIZE 48
#define MC_HOST_LAYOUT_OUTPUT_STATS_ALIGN 8
#define MC_HOST_LAYOUT_OUTPUT_STATS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_OUTPUT_STATS_RECORDS_OFFSET 8
#define MC_HOST_LAYOUT_OUTPUT_STATS_RETAINED_BYTES_OFFSET 16
#define MC_HOST_LAYOUT_OUTPUT_STATS_DROPPED_BYTES_OFFSET 24
#define MC_HOST_LAYOUT_OUTPUT_STATS_LEASED_RECORDS_OFFSET 32
#define MC_HOST_LAYOUT_OUTPUT_STATS_FLAGS_OFFSET 40
#define MC_HOST_LAYOUT_OUTPUT_STATS_RESERVED_OFFSET 44
#define MC_HOST_LAYOUT_OUTPUT_API_SIZE 48
#define MC_HOST_LAYOUT_OUTPUT_API_ALIGN 8
#define MC_HOST_LAYOUT_OUTPUT_API_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_OUTPUT_API_NEXT_OFFSET 8
#define MC_HOST_LAYOUT_OUTPUT_API_READ_OFFSET 16
#define MC_HOST_LAYOUT_OUTPUT_API_ACK_OFFSET 24
#define MC_HOST_LAYOUT_OUTPUT_API_RELEASE_OFFSET 32
#define MC_HOST_LAYOUT_OUTPUT_API_STATS_OFFSET 40
#define MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_SIZE 24
#define MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_ALIGN 8
#define MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_MODE_OFFSET 8
#define MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_RESERVED_OFFSET 12
#define MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_BASELINE_OFFSET 16
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_SIZE 128
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_ALIGN 8
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_KIND_OFFSET 8
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_MCSN_VERSION_OFFSET 12
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_KERNEL_DIGEST_OFFSET 16
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_BASELINE_ID_OFFSET 48
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_CONTENT_DIGEST_OFFSET 80
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_LENGTH_OFFSET 112
#define MC_HOST_LAYOUT_SNAPSHOT_INFO_MEMORY_LEN_OFFSET 120
#define MC_HOST_LAYOUT_SNAPSHOT_READINESS_SIZE 32
#define MC_HOST_LAYOUT_SNAPSHOT_READINESS_ALIGN 4
#define MC_HOST_LAYOUT_SNAPSHOT_READINESS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_SNAPSHOT_READINESS_INFLIGHT_EGRESS_OFFSET 8
#define MC_HOST_LAYOUT_SNAPSHOT_READINESS_PENDING_COMMITS_OFFSET 12
#define MC_HOST_LAYOUT_SNAPSHOT_READINESS_PENDING_OPERATIONS_OFFSET 16
#define MC_HOST_LAYOUT_SNAPSHOT_READINESS_PENDING_EFFECTS_OFFSET 20
#define MC_HOST_LAYOUT_SNAPSHOT_READINESS_PENDING_TRANSACTIONS_OFFSET 24
#define MC_HOST_LAYOUT_SNAPSHOT_READINESS_RESERVED_OFFSET 28
#define MC_HOST_LAYOUT_SNAPSHOT_API_SIZE 80
#define MC_HOST_LAYOUT_SNAPSHOT_API_ALIGN 8
#define MC_HOST_LAYOUT_SNAPSHOT_API_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_SNAPSHOT_API_BASELINE_IMPORT_OFFSET 8
#define MC_HOST_LAYOUT_SNAPSHOT_API_BASELINE_CLOSE_OFFSET 16
#define MC_HOST_LAYOUT_SNAPSHOT_API_BEGIN_OFFSET 24
#define MC_HOST_LAYOUT_SNAPSHOT_API_READ_OFFSET 32
#define MC_HOST_LAYOUT_SNAPSHOT_API_FINISH_OFFSET 40
#define MC_HOST_LAYOUT_SNAPSHOT_API_CANCEL_OFFSET 48
#define MC_HOST_LAYOUT_SNAPSHOT_API_INSPECT_OFFSET 56
#define MC_HOST_LAYOUT_SNAPSHOT_API_READINESS_OFFSET 64
#define MC_HOST_LAYOUT_SNAPSHOT_API_ATTACHMENT_MANIFEST_OFFSET 72
#define MC_HOST_LAYOUT_OPERATION_STATUS_SIZE 32
#define MC_HOST_LAYOUT_OPERATION_STATUS_ALIGN 8
#define MC_HOST_LAYOUT_OPERATION_STATUS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_OPERATION_STATUS_KIND_OFFSET 8
#define MC_HOST_LAYOUT_OPERATION_STATUS_STATE_OFFSET 12
#define MC_HOST_LAYOUT_OPERATION_STATUS_ERROR_OFFSET 16
#define MC_HOST_LAYOUT_OPERATION_STATUS_RESERVED_OFFSET 20
#define MC_HOST_LAYOUT_OPERATION_STATUS_RESULT_LENGTH_OFFSET 24
#define MC_HOST_LAYOUT_OPERATION_API_SIZE 56
#define MC_HOST_LAYOUT_OPERATION_API_ALIGN 8
#define MC_HOST_LAYOUT_OPERATION_API_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_OPERATION_API_START_OFFSET 8
#define MC_HOST_LAYOUT_OPERATION_API_POLL_OFFSET 16
#define MC_HOST_LAYOUT_OPERATION_API_RESULT_OFFSET 24
#define MC_HOST_LAYOUT_OPERATION_API_PEEK_OFFSET 32
#define MC_HOST_LAYOUT_OPERATION_API_CANCEL_OFFSET 40
#define MC_HOST_LAYOUT_OPERATION_API_CLOSE_OFFSET 48
#define MC_HOST_LAYOUT_ATTACHMENT_API_SIZE 56
#define MC_HOST_LAYOUT_ATTACHMENT_API_ALIGN 8
#define MC_HOST_LAYOUT_ATTACHMENT_API_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_ATTACHMENT_API_PLAN_CREATE_OFFSET 8
#define MC_HOST_LAYOUT_ATTACHMENT_API_PLAN_ADD_OFFSET 16
#define MC_HOST_LAYOUT_ATTACHMENT_API_PLAN_SEAL_OFFSET 24
#define MC_HOST_LAYOUT_ATTACHMENT_API_PLAN_CLOSE_OFFSET 32
#define MC_HOST_LAYOUT_ATTACHMENT_API_VM_ATTACH_UPDATE_OFFSET 40
#define MC_HOST_LAYOUT_ATTACHMENT_API_VM_ATTACHMENT_MANIFEST_OFFSET 48
#define MC_HOST_LAYOUT_EFFECT_INFO_SIZE 24
#define MC_HOST_LAYOUT_EFFECT_INFO_ALIGN 8
#define MC_HOST_LAYOUT_EFFECT_INFO_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_EFFECT_INFO_KIND_OFFSET 8
#define MC_HOST_LAYOUT_EFFECT_INFO_RESERVED_OFFSET 12
#define MC_HOST_LAYOUT_EFFECT_INFO_LENGTH_OFFSET 16
#define MC_HOST_LAYOUT_EFFECT_STATS_SIZE 80
#define MC_HOST_LAYOUT_EFFECT_STATS_ALIGN 8
#define MC_HOST_LAYOUT_EFFECT_STATS_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_EFFECT_STATS_QUEUED_EVENTS_OFFSET 8
#define MC_HOST_LAYOUT_EFFECT_STATS_LEASED_EVENTS_OFFSET 16
#define MC_HOST_LAYOUT_EFFECT_STATS_QUEUED_BYTES_OFFSET 24
#define MC_HOST_LAYOUT_EFFECT_STATS_RESPONSE_BYTES_OFFSET 32
#define MC_HOST_LAYOUT_EFFECT_STATS_REFUSED_HOST_CALL_OFFSET 40
#define MC_HOST_LAYOUT_EFFECT_STATS_REFUSED_PERSIST_OFFSET 48
#define MC_HOST_LAYOUT_EFFECT_STATS_REFUSED_HTTP_OFFSET 56
#define MC_HOST_LAYOUT_EFFECT_STATS_REFUSED_WS_OFFSET 64
#define MC_HOST_LAYOUT_EFFECT_STATS_REFUSED_APPROVAL_OFFSET 72
#define MC_HOST_LAYOUT_EFFECT_API_SIZE 136
#define MC_HOST_LAYOUT_EFFECT_API_ALIGN 8
#define MC_HOST_LAYOUT_EFFECT_API_HEADER_OFFSET 0
#define MC_HOST_LAYOUT_EFFECT_API_NEXT_OFFSET 8
#define MC_HOST_LAYOUT_EFFECT_API_READ_OFFSET 16
#define MC_HOST_LAYOUT_EFFECT_API_RELEASE_OFFSET 24
#define MC_HOST_LAYOUT_EFFECT_API_RESPOND_OFFSET 32
#define MC_HOST_LAYOUT_EFFECT_API_ACK_OFFSET 40
#define MC_HOST_LAYOUT_EFFECT_API_RESPONSE_BEGIN_OFFSET 48
#define MC_HOST_LAYOUT_EFFECT_API_RESPONSE_WRITE_OFFSET 56
#define MC_HOST_LAYOUT_EFFECT_API_RESPONSE_FINISH_OFFSET 64
#define MC_HOST_LAYOUT_EFFECT_API_RESPONSE_ABORT_OFFSET 72
#define MC_HOST_LAYOUT_EFFECT_API_SOCKET_PUSH_OFFSET 80
#define MC_HOST_LAYOUT_EFFECT_API_SOCKET_CLOSE_OFFSET 88
#define MC_HOST_LAYOUT_EFFECT_API_INFO_OFFSET 96
#define MC_HOST_LAYOUT_EFFECT_API_CANCEL_OFFSET 104
#define MC_HOST_LAYOUT_EFFECT_API_STATS_OFFSET 112
#define MC_HOST_LAYOUT_EFFECT_API_TRANSCRIPT_NEXT_OFFSET 120
#define MC_HOST_LAYOUT_EFFECT_API_TRANSCRIPT_ACK_OFFSET 128
#define MC_HOST_OK UINT32_C(0)
#define MC_HOST_BUFFER_TOO_SMALL UINT32_C(1)
#define MC_HOST_WOULD_BLOCK UINT32_C(2)
#define MC_HOST_INVALID_ARGUMENT UINT32_C(3)
#define MC_HOST_INVALID_STATE UINT32_C(4)
#define MC_HOST_STALE_HANDLE UINT32_C(5)
#define MC_HOST_WRONG_KIND UINT32_C(6)
#define MC_HOST_BUSY UINT32_C(7)
#define MC_HOST_NOT_QUIESCENT UINT32_C(8)
#define MC_HOST_UNSUPPORTED UINT32_C(9)
#define MC_HOST_UNSUPPORTED_ABI UINT32_C(10)
#define MC_HOST_CANCELLED UINT32_C(11)
#define MC_HOST_RESOURCE_LIMIT UINT32_C(12)
#define MC_HOST_KERNEL_ERROR UINT32_C(13)
#define MC_HOST_GUEST_TRAP UINT32_C(14)
#define MC_HOST_SNAPSHOT_ERROR UINT32_C(15)
#define MC_HOST_HOST_IO UINT32_C(16)
#define MC_HOST_POISONED UINT32_C(17)
#define MC_HOST_INTERNAL UINT32_C(18)
#define MC_HOST_OUTPUT_LOST UINT32_C(19)
#define MC_HOST_IFACE_VM_V1 UINT32_C(1)
#define MC_HOST_IFACE_OPERATION_V1 UINT32_C(2)
#define MC_HOST_IFACE_EFFECT_V1 UINT32_C(3)
#define MC_HOST_IFACE_SNAPSHOT_V1 UINT32_C(4)
#define MC_HOST_IFACE_NATIVE_CAPS_V1 UINT32_C(5)
#define MC_HOST_IFACE_CATALOG_V1 UINT32_C(6)
#define MC_HOST_IFACE_PERF_V1 UINT32_C(7)
#define MC_HOST_IFACE_OUTPUT_V1 UINT32_C(8)
#define MC_HOST_IFACE_ATTACHMENT_V1 UINT32_C(9)
#define MC_HOST_FEATURE_VM_BASIC UINT64_C(1)
#define MC_HOST_FEATURE_OUTPUT UINT64_C(2)
#define MC_HOST_FEATURE_SNAPSHOT UINT64_C(4)
#define MC_HOST_FEATURE_EXEC_OPERATION UINT64_C(8)
#define MC_HOST_FEATURE_FS_OPERATION UINT64_C(16)
#define MC_HOST_FEATURE_AUTOCOMPLETE_OPERATION UINT64_C(32)
#define MC_HOST_FEATURE_SERVICE_OPERATION UINT64_C(64)
#define MC_HOST_FEATURE_COMMIT_OPERATION UINT64_C(128)
#define MC_HOST_FEATURE_ATTACHMENT_PLAN UINT64_C(256)
#define MC_HOST_FEATURE_HOST_CALL_EFFECT UINT64_C(512)
#define MC_HOST_FEATURE_MOUNT_OPERATION UINT64_C(1024)
#define MC_HOST_FEATURE_PERSIST_EFFECT UINT64_C(2048)
#define MC_HOST_FEATURE_HTTP_EFFECT UINT64_C(4096)
#define MC_HOST_FEATURE_WS_EFFECT UINT64_C(8192)
#define MC_HOST_FEATURE_NATIVE_CONNECTION UINT64_C(16384)
#define MC_HOST_FEATURE_LIVE_ATTACHMENT UINT64_C(32768)
#define MC_HOST_DEFAULT_MAX_VMS UINT32_C(16)
#define MC_HOST_HARD_MAX_VMS UINT32_C(16)
#define MC_HOST_DEFAULT_MAX_DRIVE_TICKS UINT32_C(1000)
#define MC_HOST_HARD_MAX_DRIVE_TICKS UINT32_C(1000)
#define MC_HOST_DEFAULT_MAX_OUTPUT_BYTES UINT32_C(4194304)
#define MC_HOST_HARD_MAX_OUTPUT_BYTES UINT32_C(4194304)
#define MC_HOST_DEFAULT_MAX_OUTPUT_RECORDS UINT32_C(4096)
#define MC_HOST_HARD_MAX_OUTPUT_RECORDS UINT32_C(4096)
#define MC_HOST_DEFAULT_MAX_SNAPSHOT_BYTES UINT32_C(536870912)
#define MC_HOST_HARD_MAX_SNAPSHOT_BYTES UINT32_C(536870912)
#define MC_HOST_DEFAULT_MAX_SNAPSHOT_TOTAL_BYTES UINT32_C(1073741824)
#define MC_HOST_HARD_MAX_SNAPSHOT_TOTAL_BYTES UINT32_C(1073741824)
#define MC_HOST_DEFAULT_MAX_OPERATIONS_PER_VM UINT32_C(64)
#define MC_HOST_HARD_MAX_OPERATIONS_PER_VM UINT32_C(64)
#define MC_HOST_DEFAULT_MAX_EFFECT_EVENTS UINT32_C(256)
#define MC_HOST_HARD_MAX_EFFECT_EVENTS UINT32_C(256)
#define MC_HOST_DEFAULT_MAX_EFFECT_BYTES UINT32_C(4194304)
#define MC_HOST_HARD_MAX_EFFECT_BYTES UINT32_C(4194304)
#define MC_HOST_DEFAULT_MAX_EFFECT_RESPONSE_BYTES UINT32_C(67108864)
#define MC_HOST_HARD_MAX_EFFECT_RESPONSE_BYTES UINT32_C(67108864)
#define MC_HOST_DEFAULT_MAX_WS_SOCKETS UINT32_C(256)
#define MC_HOST_HARD_MAX_WS_SOCKETS UINT32_C(256)
#define MC_HOST_DEFAULT_MAX_WS_INCOMING_BYTES UINT32_C(16777216)
#define MC_HOST_HARD_MAX_WS_INCOMING_BYTES UINT32_C(16777216)
#define MC_HOST_MIN_NATIVE_STACK_BYTES UINT32_C(8388608)
#define MC_HOST_DEFAULT_MAX_KERNELS UINT32_C(64)
#define MC_HOST_HARD_MAX_KERNELS UINT32_C(64)
#define MC_HOST_DEFAULT_MAX_KERNEL_BYTES UINT32_C(268435456)
#define MC_HOST_HARD_MAX_KERNEL_BYTES UINT32_C(268435456)
#define MC_HOST_DEFAULT_MAX_COMPILED_MODULES UINT32_C(64)
#define MC_HOST_HARD_MAX_COMPILED_MODULES UINT32_C(64)
#define MC_HOST_DEFAULT_MAX_IMAGE_BYTES UINT32_C(268435456)
#define MC_HOST_HARD_MAX_IMAGE_BYTES UINT32_C(268435456)
#define MC_HOST_DEFAULT_MAX_SNAPSHOT_HANDLES UINT32_C(64)
#define MC_HOST_HARD_MAX_SNAPSHOT_HANDLES UINT32_C(64)
#define MC_HOST_DEFAULT_MAX_OPERATION_RESULT_BYTES UINT32_C(16777216)
#define MC_HOST_HARD_MAX_OPERATION_RESULT_BYTES UINT32_C(16777216)
#define MC_HOST_DEFAULT_MAX_MESSAGE_BYTES UINT32_C(18874368)
#define MC_HOST_HARD_MAX_MESSAGE_BYTES UINT32_C(18874368)
#define MC_HOST_HARD_MAX_PLAN_DEFINITION_BYTES UINT32_C(524288)
#define MC_HOST_DEFAULT_MAX_EFFECT_EVENT_BYTES UINT32_C(4194304)
#define MC_HOST_HARD_MAX_EFFECT_EVENT_BYTES UINT32_C(4194304)
#define MC_HOST_DEFAULT_MAX_EFFECT_SINGLE_RESPONSE_BYTES UINT32_C(16777216)
#define MC_HOST_HARD_MAX_EFFECT_SINGLE_RESPONSE_BYTES UINT32_C(16777216)
#define MC_HOST_TICK_WAITING UINT32_C(0)
#define MC_HOST_TICK_RUNNABLE UINT32_C(1)
#define MC_HOST_TICK_EXITED UINT32_C(2)
#define MC_HOST_VM_RUNNABLE UINT32_C(1)
#define MC_HOST_VM_WAITING UINT32_C(2)
#define MC_HOST_VM_EXITED UINT32_C(3)
#define MC_HOST_VM_POISONED UINT32_C(4)
#define MC_HOST_VM_CLOSING UINT32_C(5)
#define MC_HOST_VM_FLAG_HAS_EXIT_CODE UINT32_C(1)
#define MC_HOST_VM_FLAG_AT_PROMPT UINT32_C(2)
#define MC_HOST_VM_CREATE_DETERMINISTIC UINT32_C(1)
#define MC_HOST_VM_RESTORE_DETERMINISTIC UINT32_C(1)
#define MC_HOST_RESTORE_STRICT UINT32_C(1)
#define MC_HOST_RESTORE_DETACHED UINT32_C(2)
#define MC_HOST_DRIVE_STOP_BUDGET UINT32_C(1)
#define MC_HOST_DRIVE_STOP_WAITING UINT32_C(2)
#define MC_HOST_DRIVE_STOP_EXITED UINT32_C(3)
#define MC_HOST_DRIVE_STOP_OUTPUT UINT32_C(4)
#define MC_HOST_DRIVE_STOP_EFFECT UINT32_C(5)
#define MC_HOST_DRIVE_STOP_POISONED UINT32_C(6)
#define MC_HOST_DRIVE_STOP_OPERATION UINT32_C(7)
#define MC_HOST_DRIVE_ON_OUTPUT UINT64_C(1)
#define MC_HOST_DRIVE_ON_OPERATION UINT64_C(2)
#define MC_HOST_DRIVE_ON_EFFECT UINT64_C(4)
#define MC_HOST_VM_FLAG_OUTPUT_READY UINT32_C(4)
#define MC_HOST_VM_FLAG_OPERATION_READY UINT32_C(8)
#define MC_HOST_VM_FLAG_OPERATION_PENDING UINT32_C(16)
#define MC_HOST_VM_FLAG_EFFECT_READY UINT32_C(32)
#define MC_HOST_VM_FLAG_OUTPUT_LOST UINT32_C(64)
#define MC_HOST_OUTPUT_DATA UINT32_C(1)
#define MC_HOST_OUTPUT_GAP UINT32_C(2)
#define MC_HOST_OUTPUT_ABOVE_HIGH_WATER UINT32_C(1)
#define MC_HOST_OUTPUT_POISONED UINT32_C(2)
#define MC_HOST_SNAPSHOT_FULL UINT32_C(1)
#define MC_HOST_SNAPSHOT_INCREMENTAL UINT32_C(2)
#define MC_HOST_OP_NEW UINT32_C(0)
#define MC_HOST_OP_PENDING UINT32_C(1)
#define MC_HOST_OP_READY UINT32_C(2)
#define MC_HOST_OP_FAILED UINT32_C(3)
#define MC_HOST_OP_CANCELLED UINT32_C(4)
#define MC_HOST_OP_EXEC_SHELL UINT32_C(1)
#define MC_HOST_OP_EXEC_DIRECT UINT32_C(2)
#define MC_HOST_OP_FS_READ UINT32_C(3)
#define MC_HOST_OP_FS_WRITE UINT32_C(4)
#define MC_HOST_OP_FS_READDIR UINT32_C(5)
#define MC_HOST_OP_FS_STAT UINT32_C(6)
#define MC_HOST_OP_FS_READLINK UINT32_C(7)
#define MC_HOST_OP_FS_MKDIR UINT32_C(8)
#define MC_HOST_OP_FS_UNLINK UINT32_C(9)
#define MC_HOST_OP_FS_CHMOD UINT32_C(10)
#define MC_HOST_OP_FS_SYMLINK UINT32_C(11)
#define MC_HOST_OP_MOUNT UINT32_C(12)
#define MC_HOST_OP_UNMOUNT UINT32_C(13)
#define MC_HOST_OP_AUTOCOMPLETE UINT32_C(14)
#define MC_HOST_OP_SERVICE_CALL UINT32_C(15)
#define MC_HOST_OP_COMMIT_LAYER UINT32_C(16)
#define MC_HOST_OP_CATALOG_INJECT UINT32_C(17)
#define MC_HOST_ATTACHMENT_HOST_CALL UINT32_C(1)
#define MC_HOST_ATTACHMENT_PERSIST UINT32_C(2)
#define MC_HOST_ATTACHMENT_HTTP_APPLICATION UINT32_C(3)
#define MC_HOST_ATTACHMENT_HOST_TOOL UINT32_C(4)
#define MC_HOST_ATTACHMENT_CONNECTION UINT32_C(5)
#define MC_HOST_ATTACHMENT_CONNECTION_POLICY UINT32_C(6)
#define MC_HOST_EVENT_HTTP_REQUEST UINT32_C(1)
#define MC_HOST_EVENT_HTTP_CANCEL UINT32_C(2)
#define MC_HOST_EVENT_WS_CONNECT UINT32_C(3)
#define MC_HOST_EVENT_WS_SEND UINT32_C(4)
#define MC_HOST_EVENT_WS_CLOSE UINT32_C(5)
#define MC_HOST_EVENT_HOST_CALL UINT32_C(6)
#define MC_HOST_EVENT_HOST_CALL_CLOSE UINT32_C(7)
#define MC_HOST_EVENT_PERSIST_GET UINT32_C(8)
#define MC_HOST_EVENT_PERSIST_PUT UINT32_C(9)
#define MC_HOST_EVENT_PERSIST_DELETE UINT32_C(10)
#define MC_HOST_EVENT_PERSIST_LIST UINT32_C(11)
#define MC_HOST_EVENT_TOOL_APPROVAL UINT32_C(12)
#define MC_HOST_EVENT_OPERATION_CANCEL UINT32_C(13)
#define MC_HOST_WS_CLOSE_GUEST UINT32_C(1)
#define MC_HOST_WS_CLOSE_VM UINT32_C(2)
#define MC_HOST_TRANSCRIPT_REQUEST UINT32_C(1)
#define MC_HOST_TRANSCRIPT_RESPONSE UINT32_C(2)
#define MC_HOST_TRANSCRIPT_WS_INBOUND UINT32_C(3)
#define MC_HOST_TRANSCRIPT_WS_CLOSE UINT32_C(4)


/* Portable message codec. Decoded byte views borrow the input frame. */
#ifndef MC_HOST_WIRE_COMMON
#define MC_HOST_WIRE_COMMON
#include <stddef.h>
#include <string.h>

typedef enum mc_host_wire_result {
    MC_HOST_WIRE_OK = 0,
    MC_HOST_WIRE_INVALID_ARGUMENT = 1,
    MC_HOST_WIRE_BUFFER_TOO_SMALL = 2,
    MC_HOST_WIRE_WRONG_MESSAGE = 3,
    MC_HOST_WIRE_UNSUPPORTED_VERSION = 4,
    MC_HOST_WIRE_TRUNCATED = 5,
    MC_HOST_WIRE_INVALID_UTF8 = 6,
    MC_HOST_WIRE_INVALID_PRESENCE = 7,
    MC_HOST_WIRE_TRAILING_BYTES = 8,
    MC_HOST_WIRE_OVERFLOW = 9,
    MC_HOST_WIRE_NON_CANONICAL_MAP = 10,
    MC_HOST_WIRE_TOO_LARGE = 11
} mc_host_wire_result;

typedef struct mc_host_wire_bytes {
    const uint8_t *data;
    uint32_t len;
} mc_host_wire_bytes;

/* data contains count consecutive u32-length-prefixed nested frames. */
typedef struct mc_host_wire_list {
    const uint8_t *data;
    size_t len;
    uint32_t count;
} mc_host_wire_list;

typedef mc_host_wire_list mc_host_wire_map;

typedef struct mc_host_wire_reader {
    const uint8_t *data;
    size_t len;
    size_t off;
    mc_host_wire_result error;
} mc_host_wire_reader;

typedef struct mc_host_wire_writer {
    uint8_t *data;
    size_t cap;
    size_t off;
    mc_host_wire_result error;
} mc_host_wire_writer;

static inline int mc_host_wire_take(mc_host_wire_reader *r, size_t n, const uint8_t **p) {
    if (r->error != MC_HOST_WIRE_OK) return 0;
    if (r->off > r->len || n > r->len - r->off) {
        r->error = MC_HOST_WIRE_TRUNCATED;
        return 0;
    }
    *p = r->data + r->off;
    r->off += n;
    return 1;
}

static inline uint8_t mc_host_wire_get_u8(mc_host_wire_reader *r) {
    const uint8_t *p = NULL;
    return mc_host_wire_take(r, 1, &p) ? p[0] : 0;
}

static inline uint16_t mc_host_wire_get_u16(mc_host_wire_reader *r) {
    const uint8_t *p = NULL;
    return mc_host_wire_take(r, 2, &p) ? (uint16_t)(p[0] | (uint16_t)p[1] << 8) : 0;
}

static inline uint32_t mc_host_wire_get_u32(mc_host_wire_reader *r) {
    const uint8_t *p = NULL;
    if (!mc_host_wire_take(r, 4, &p)) return 0;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline uint64_t mc_host_wire_get_u64(mc_host_wire_reader *r) {
    uint64_t lo = mc_host_wire_get_u32(r);
    uint64_t hi = mc_host_wire_get_u32(r);
    return lo | hi << 32;
}

static inline uint8_t mc_host_wire_get_flag(mc_host_wire_reader *r) {
    uint8_t value = mc_host_wire_get_u8(r);
    if (r->error == MC_HOST_WIRE_OK && value > 1) r->error = MC_HOST_WIRE_INVALID_PRESENCE;
    return value;
}

static inline int mc_host_wire_utf8(const uint8_t *s, size_t n) {
    for (size_t i = 0; i < n;) {
        uint8_t a = s[i++];
        if (a < 0x80) continue;
        if (a < 0xC2 || a > 0xF4 || i == n) return 0;
        uint8_t b = s[i++];
        if (b < 0x80 || b > 0xBF) return 0;
        if (a < 0xE0) continue;
        if (i == n) return 0;
        uint8_t c = s[i++];
        if (c < 0x80 || c > 0xBF || (a == 0xE0 && b < 0xA0) ||
            (a == 0xED && b > 0x9F)) return 0;
        if (a < 0xF0) continue;
        if (i == n) return 0;
        uint8_t d = s[i++];
        if (d < 0x80 || d > 0xBF || (a == 0xF0 && b < 0x90) ||
            (a == 0xF4 && b > 0x8F)) return 0;
    }
    return 1;
}

static inline mc_host_wire_bytes mc_host_wire_get_bytes(mc_host_wire_reader *r, int utf8) {
    mc_host_wire_bytes value = {0};
    uint32_t len = mc_host_wire_get_u32(r);
    const uint8_t *p = NULL;
    if (!mc_host_wire_take(r, len, &p)) return value;
    if (utf8 && !mc_host_wire_utf8(p, len)) {
        r->error = MC_HOST_WIRE_INVALID_UTF8;
        return value;
    }
    value.data = p;
    value.len = len;
    return value;
}

static inline int mc_host_wire_less(mc_host_wire_bytes a, mc_host_wire_bytes b) {
    size_t n = a.len < b.len ? a.len : b.len;
    int order = n ? memcmp(a.data, b.data, n) : 0;
    return order < 0 || (order == 0 && a.len < b.len);
}

static inline mc_host_wire_map mc_host_wire_get_map(mc_host_wire_reader *r) {
    mc_host_wire_map map = {0};
    map.count = mc_host_wire_get_u32(r);
    if (r->error != MC_HOST_WIRE_OK) return map;
    map.data = r->data + r->off;
    size_t start = r->off;
    mc_host_wire_bytes previous = {0};
    for (uint32_t i = 0; i < map.count && r->error == MC_HOST_WIRE_OK; ++i) {
        mc_host_wire_bytes key = mc_host_wire_get_bytes(r, 1);
        if (r->error == MC_HOST_WIRE_OK && i && !mc_host_wire_less(previous, key))
            r->error = MC_HOST_WIRE_NON_CANONICAL_MAP;
        mc_host_wire_get_bytes(r, 1);
        previous = key;
    }
    map.len = r->off - start;
    return map;
}

static inline void mc_host_wire_put(mc_host_wire_writer *w, const uint8_t *p, size_t n) {
    if (w->error != MC_HOST_WIRE_OK) return;
    if (n > SIZE_MAX - w->off) {
        w->error = MC_HOST_WIRE_OVERFLOW;
        return;
    }
    if (n && !p) {
        w->error = MC_HOST_WIRE_INVALID_ARGUMENT;
        return;
    }
    if (w->data && n) {
        if (w->off > w->cap || n > w->cap - w->off) {
            w->error = MC_HOST_WIRE_BUFFER_TOO_SMALL;
            return;
        }
        memcpy(w->data + w->off, p, n);
    }
    w->off += n;
}

static inline void mc_host_wire_put_u8(mc_host_wire_writer *w, uint8_t value) {
    mc_host_wire_put(w, &value, 1);
}

static inline void mc_host_wire_put_u16(mc_host_wire_writer *w, uint16_t value) {
    uint8_t p[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    mc_host_wire_put(w, p, 2);
}

static inline void mc_host_wire_put_u32(mc_host_wire_writer *w, uint32_t value) {
    uint8_t p[4] = {(uint8_t)value, (uint8_t)(value >> 8),
                    (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
    mc_host_wire_put(w, p, 4);
}

static inline void mc_host_wire_put_u64(mc_host_wire_writer *w, uint64_t value) {
    mc_host_wire_put_u32(w, (uint32_t)value);
    mc_host_wire_put_u32(w, (uint32_t)(value >> 32));
}

static inline void mc_host_wire_put_bytes(mc_host_wire_writer *w, mc_host_wire_bytes value, int utf8) {
    if (value.len && !value.data) { w->error = MC_HOST_WIRE_INVALID_ARGUMENT; return; }
    if (utf8 && !mc_host_wire_utf8(value.data, value.len)) {
        w->error = MC_HOST_WIRE_INVALID_UTF8;
        return;
    }
    mc_host_wire_put_u32(w, value.len);
    mc_host_wire_put(w, value.data, value.len);
}

/* Iterate a decoded list without allocating. Returns 1 for an item, 0 at end, -1 on corruption. */
static inline int mc_host_wire_list_next(mc_host_wire_list list, size_t *offset,
                                         mc_host_wire_bytes *item) {
    if (!offset || !item || *offset > list.len) return -1;
    if (*offset == list.len) return 0;
    if (list.len - *offset < 4) return -1;
    mc_host_wire_reader r = {list.data, list.len, *offset, MC_HOST_WIRE_OK};
    *item = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return -1;
    *offset = r.off;
    return 1;
}

static inline mc_host_wire_result mc_host_wire_list_append(mc_host_wire_list *list,
        uint8_t *buffer, size_t capacity, mc_host_wire_bytes item) {
    if (!list || !buffer || (item.len && !item.data) ||
        (list->data && list->data != buffer) || list->count == UINT32_MAX)
        return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (item.len) {
        uintptr_t start = (uintptr_t)item.data;
        uintptr_t destination = (uintptr_t)buffer;
        if ((start >= destination && start - destination < capacity) ||
            (start < destination && item.len > destination - start))
            return MC_HOST_WIRE_INVALID_ARGUMENT;
    }
    if (list->len > capacity || capacity - list->len < 4 ||
        item.len > capacity - list->len - 4)
        return MC_HOST_WIRE_BUFFER_TOO_SMALL;
    mc_host_wire_writer writer = {buffer, capacity, list->len, MC_HOST_WIRE_OK};
    mc_host_wire_put_bytes(&writer, item, 0);
    if (writer.error != MC_HOST_WIRE_OK) return writer.error;
    list->data = buffer;
    list->len = writer.off;
    ++list->count;
    return MC_HOST_WIRE_OK;
}
#endif /* MC_HOST_WIRE_COMMON */
#define MC_HOST_WIRE_ATTACHMENT_DEFINITION_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_attachment_definition {
    uint32_t kind;
    mc_host_wire_bytes payload;
} mc_host_wire_attachment_definition;
static inline mc_host_wire_result mc_host_wire_decode_attachment_definition(const uint8_t *data, size_t len, mc_host_wire_attachment_definition *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 20 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->kind = mc_host_wire_get_u32(&r);
        out->payload = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_attachment_definition(const mc_host_wire_attachment_definition *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 20);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->kind);
            mc_host_wire_put_bytes(&w, value->payload, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_OPERATION_REQUEST_MAX_FRAME_BYTES ((size_t)UINT32_C(18874368))

typedef struct mc_host_wire_operation_request {
    uint32_t kind;
    mc_host_wire_bytes payload;
} mc_host_wire_operation_request;
static inline mc_host_wire_result mc_host_wire_decode_operation_request(const uint8_t *data, size_t len, mc_host_wire_operation_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 18874368) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 21 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->kind = mc_host_wire_get_u32(&r);
        out->payload = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_operation_request(const mc_host_wire_operation_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 21);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->kind);
            mc_host_wire_put_bytes(&w, value->payload, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 18874368) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_OPERATION_RESULT_MAX_FRAME_BYTES ((size_t)UINT32_C(16777216))

typedef struct mc_host_wire_operation_result {
    uint32_t kind;
    int32_t status;
    mc_host_wire_bytes payload;
} mc_host_wire_operation_result;
static inline mc_host_wire_result mc_host_wire_decode_operation_result(const uint8_t *data, size_t len, mc_host_wire_operation_result *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 16777216) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 22 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->kind = mc_host_wire_get_u32(&r);
        out->status = mc_host_wire_get_u32(&r);
        out->payload = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_operation_result(const mc_host_wire_operation_result *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 22);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->kind);
            mc_host_wire_put_u32(&w, (uint32_t)value->status);
            mc_host_wire_put_bytes(&w, value->payload, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 16777216) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_EFFECT_EVENT_MAX_FRAME_BYTES ((size_t)UINT32_C(4194368))

typedef struct mc_host_wire_effect_event {
    uint32_t kind;
    int64_t event_id;
    int64_t vm_id;
    uint8_t has_operation_id;
    int64_t operation_id;
    uint8_t has_socket_id;
    int64_t socket_id;
    uint32_t flags;
    mc_host_wire_bytes payload;
} mc_host_wire_effect_event;
static inline mc_host_wire_result mc_host_wire_decode_effect_event(const uint8_t *data, size_t len, mc_host_wire_effect_event *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 4194368) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 23 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->kind = mc_host_wire_get_u32(&r);
        out->event_id = mc_host_wire_get_u64(&r);
        out->vm_id = mc_host_wire_get_u64(&r);
    out->has_operation_id = mc_host_wire_get_flag(&r);
    if (out->has_operation_id) {
        out->operation_id = mc_host_wire_get_u64(&r);
    }
    out->has_socket_id = mc_host_wire_get_flag(&r);
    if (out->has_socket_id) {
        out->socket_id = mc_host_wire_get_u64(&r);
    }
        out->flags = mc_host_wire_get_u32(&r);
        out->payload = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_effect_event(const mc_host_wire_effect_event *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 23);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->kind);
            mc_host_wire_put_u64(&w, (uint64_t)value->event_id);
            mc_host_wire_put_u64(&w, (uint64_t)value->vm_id);
        if (value->has_operation_id > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_operation_id);
        if (value->has_operation_id) {
            mc_host_wire_put_u64(&w, (uint64_t)value->operation_id);
        }
        if (value->has_socket_id > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_socket_id);
        if (value->has_socket_id) {
            mc_host_wire_put_u64(&w, (uint64_t)value->socket_id);
        }
            mc_host_wire_put_u32(&w, (uint32_t)value->flags);
            mc_host_wire_put_bytes(&w, value->payload, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 4194368) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_EFFECT_RESPONSE_MAX_FRAME_BYTES ((size_t)UINT32_C(16777344))

typedef struct mc_host_wire_effect_response {
    uint32_t kind;
    int64_t event_id;
    mc_host_wire_bytes payload;
} mc_host_wire_effect_response;
static inline mc_host_wire_result mc_host_wire_decode_effect_response(const uint8_t *data, size_t len, mc_host_wire_effect_response *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 16777344) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 24 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->kind = mc_host_wire_get_u32(&r);
        out->event_id = mc_host_wire_get_u64(&r);
        out->payload = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_effect_response(const mc_host_wire_effect_response *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 24);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->kind);
            mc_host_wire_put_u64(&w, (uint64_t)value->event_id);
            mc_host_wire_put_bytes(&w, value->payload, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 16777344) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_ATTACHMENT_MANIFEST_ENTRY_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_attachment_manifest_entry {
    uint32_t kind;
    mc_host_wire_bytes identity;
    mc_host_wire_bytes digest;
} mc_host_wire_attachment_manifest_entry;
static inline mc_host_wire_result mc_host_wire_decode_attachment_manifest_entry(const uint8_t *data, size_t len, mc_host_wire_attachment_manifest_entry *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 25 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->kind = mc_host_wire_get_u32(&r);
        out->identity = mc_host_wire_get_bytes(&r, 1);
        out->digest = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_attachment_manifest_entry(const mc_host_wire_attachment_manifest_entry *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 25);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->kind);
            mc_host_wire_put_bytes(&w, value->identity, 1);
            mc_host_wire_put_bytes(&w, value->digest, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_ATTACHMENT_MANIFEST_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_attachment_manifest {
    mc_host_wire_list entries;
} mc_host_wire_attachment_manifest;
static inline mc_host_wire_result mc_host_wire_decode_attachment_manifest(const uint8_t *data, size_t len, mc_host_wire_attachment_manifest *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 26 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        {
        out->entries.count = mc_host_wire_get_u32(&r);
        out->entries.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->entries.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_attachment_manifest_entry nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_attachment_manifest_entry(item.data, item.len, &nested);
        }
        out->entries.len = r.off - list_start;
        }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_attachment_manifest(const mc_host_wire_attachment_manifest *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 26);
        mc_host_wire_put_u8(&w, 1);
            {
            mc_host_wire_reader list_reader = {value->entries.data, value->entries.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->entries.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_attachment_manifest_entry nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_attachment_manifest_entry(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->entries.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->entries.count);
            mc_host_wire_put(&w, value->entries.data, value->entries.len);
            }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_FILESYSTEM_REQUEST_MAX_FRAME_BYTES ((size_t)UINT32_C(18874368))

typedef struct mc_host_wire_filesystem_request {
    mc_host_wire_bytes path;
    uint8_t has_data;
    mc_host_wire_bytes data;
    uint8_t has_target;
    mc_host_wire_bytes target;
    uint8_t has_mode;
    uint32_t mode;
} mc_host_wire_filesystem_request;
static inline mc_host_wire_result mc_host_wire_decode_filesystem_request(const uint8_t *data, size_t len, mc_host_wire_filesystem_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 18874368) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 27 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->path = mc_host_wire_get_bytes(&r, 1);
    out->has_data = mc_host_wire_get_flag(&r);
    if (out->has_data) {
        out->data = mc_host_wire_get_bytes(&r, 0);
    }
    out->has_target = mc_host_wire_get_flag(&r);
    if (out->has_target) {
        out->target = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_mode = mc_host_wire_get_flag(&r);
    if (out->has_mode) {
        out->mode = mc_host_wire_get_u32(&r);
    }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_filesystem_request(const mc_host_wire_filesystem_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 27);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->path, 1);
        if (value->has_data > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_data);
        if (value->has_data) {
            mc_host_wire_put_bytes(&w, value->data, 0);
        }
        if (value->has_target > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_target);
        if (value->has_target) {
            mc_host_wire_put_bytes(&w, value->target, 1);
        }
        if (value->has_mode > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_mode);
        if (value->has_mode) {
            mc_host_wire_put_u32(&w, (uint32_t)value->mode);
        }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 18874368) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_LAYER_COMMIT_RESULT_MAX_FRAME_BYTES ((size_t)UINT32_C(16777216))

typedef struct mc_host_wire_layer_commit_result {
    mc_host_wire_bytes digest;
    mc_host_wire_bytes layer;
} mc_host_wire_layer_commit_result;
static inline mc_host_wire_result mc_host_wire_decode_layer_commit_result(const uint8_t *data, size_t len, mc_host_wire_layer_commit_result *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 16777216) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 28 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->digest = mc_host_wire_get_bytes(&r, 1);
        out->layer = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_layer_commit_result(const mc_host_wire_layer_commit_result *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 28);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->digest, 1);
            mc_host_wire_put_bytes(&w, value->layer, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 16777216) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_HOST_CALL_ATTACHMENT_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_host_call_attachment {
    mc_host_wire_bytes name;
    mc_host_wire_bytes identity;
} mc_host_wire_host_call_attachment;
static inline mc_host_wire_result mc_host_wire_decode_host_call_attachment(const uint8_t *data, size_t len, mc_host_wire_host_call_attachment *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 29 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->name = mc_host_wire_get_bytes(&r, 1);
        out->identity = mc_host_wire_get_bytes(&r, 1);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_host_call_attachment(const mc_host_wire_host_call_attachment *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 29);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->name, 1);
            mc_host_wire_put_bytes(&w, value->identity, 1);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_MOUNT_REQUEST_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_mount_request {
    mc_host_wire_bytes path;
    uint8_t read_only;
} mc_host_wire_mount_request;
static inline mc_host_wire_result mc_host_wire_decode_mount_request(const uint8_t *data, size_t len, mc_host_wire_mount_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 30 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->path = mc_host_wire_get_bytes(&r, 1);
        out->read_only = mc_host_wire_get_flag(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_mount_request(const mc_host_wire_mount_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 30);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->path, 1);
            if (value->read_only > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->read_only);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_HOST_CALL_EVENT_MAX_FRAME_BYTES ((size_t)UINT32_C(4194304))

typedef struct mc_host_wire_host_call_event {
    mc_host_wire_bytes name;
    mc_host_wire_bytes request;
} mc_host_wire_host_call_event;
static inline mc_host_wire_result mc_host_wire_decode_host_call_event(const uint8_t *data, size_t len, mc_host_wire_host_call_event *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 4194304) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 31 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->name = mc_host_wire_get_bytes(&r, 1);
        out->request = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_host_call_event(const mc_host_wire_host_call_event *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 31);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->name, 1);
            mc_host_wire_put_bytes(&w, value->request, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 4194304) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_HOST_CALL_RESPONSE_MAX_FRAME_BYTES ((size_t)UINT32_C(16777216))

typedef struct mc_host_wire_host_call_response {
    uint8_t failed;
    mc_host_wire_bytes body;
} mc_host_wire_host_call_response;
static inline mc_host_wire_result mc_host_wire_decode_host_call_response(const uint8_t *data, size_t len, mc_host_wire_host_call_response *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 16777216) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 32 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->failed = mc_host_wire_get_flag(&r);
        out->body = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_host_call_response(const mc_host_wire_host_call_response *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 32);
        mc_host_wire_put_u8(&w, 1);
            if (value->failed > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->failed);
            mc_host_wire_put_bytes(&w, value->body, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 16777216) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_EFFECT_CANCEL_MAX_FRAME_BYTES ((size_t)UINT32_C(64))

typedef struct mc_host_wire_effect_cancel {
    int64_t original_event_id;
} mc_host_wire_effect_cancel;
static inline mc_host_wire_result mc_host_wire_decode_effect_cancel(const uint8_t *data, size_t len, mc_host_wire_effect_cancel *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 64) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 33 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->original_event_id = mc_host_wire_get_u64(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_effect_cancel(const mc_host_wire_effect_cancel *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 33);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u64(&w, (uint64_t)value->original_event_id);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 64) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_PERSIST_ATTACHMENT_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_persist_attachment {
    mc_host_wire_bytes identity;
} mc_host_wire_persist_attachment;
static inline mc_host_wire_result mc_host_wire_decode_persist_attachment(const uint8_t *data, size_t len, mc_host_wire_persist_attachment *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 34 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->identity = mc_host_wire_get_bytes(&r, 1);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_persist_attachment(const mc_host_wire_persist_attachment *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 34);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->identity, 1);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_PERSIST_EFFECT_REQUEST_MAX_FRAME_BYTES ((size_t)UINT32_C(4194304))

typedef struct mc_host_wire_persist_effect_request {
    mc_host_wire_bytes key;
    uint8_t has_value;
    mc_host_wire_bytes value;
} mc_host_wire_persist_effect_request;
static inline mc_host_wire_result mc_host_wire_decode_persist_effect_request(const uint8_t *data, size_t len, mc_host_wire_persist_effect_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 4194304) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 35 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->key = mc_host_wire_get_bytes(&r, 0);
    out->has_value = mc_host_wire_get_flag(&r);
    if (out->has_value) {
        out->value = mc_host_wire_get_bytes(&r, 0);
    }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_persist_effect_request(const mc_host_wire_persist_effect_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 35);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->key, 0);
        if (value->has_value > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_value);
        if (value->has_value) {
            mc_host_wire_put_bytes(&w, value->value, 0);
        }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 4194304) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_PERSIST_EFFECT_RESPONSE_MAX_FRAME_BYTES ((size_t)UINT32_C(16777216))

typedef struct mc_host_wire_persist_effect_response {
    uint8_t failed;
    mc_host_wire_bytes body;
} mc_host_wire_persist_effect_response;
static inline mc_host_wire_result mc_host_wire_decode_persist_effect_response(const uint8_t *data, size_t len, mc_host_wire_persist_effect_response *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 16777216) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 36 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->failed = mc_host_wire_get_flag(&r);
        out->body = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_persist_effect_response(const mc_host_wire_persist_effect_response *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 36);
        mc_host_wire_put_u8(&w, 1);
            if (value->failed > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->failed);
            mc_host_wire_put_bytes(&w, value->body, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 16777216) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_HTTP_APPLICATION_ATTACHMENT_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_http_application_attachment {
    mc_host_wire_bytes identity;
} mc_host_wire_http_application_attachment;
static inline mc_host_wire_result mc_host_wire_decode_http_application_attachment(const uint8_t *data, size_t len, mc_host_wire_http_application_attachment *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 37 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->identity = mc_host_wire_get_bytes(&r, 1);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_http_application_attachment(const mc_host_wire_http_application_attachment *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 37);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->identity, 1);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_HTTP_EFFECT_REQUEST_MAX_FRAME_BYTES ((size_t)UINT32_C(4194304))

typedef struct mc_host_wire_http_effect_request {
    mc_host_wire_bytes request;
} mc_host_wire_http_effect_request;
static inline mc_host_wire_result mc_host_wire_decode_http_effect_request(const uint8_t *data, size_t len, mc_host_wire_http_effect_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 4194304) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 38 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->request = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_http_effect_request(const mc_host_wire_http_effect_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 38);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->request, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 4194304) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_HTTP_EFFECT_RESPONSE_MAX_FRAME_BYTES ((size_t)UINT32_C(16777216))

typedef struct mc_host_wire_http_effect_response {
    uint8_t failed;
    mc_host_wire_bytes head;
    mc_host_wire_bytes body;
} mc_host_wire_http_effect_response;
static inline mc_host_wire_result mc_host_wire_decode_http_effect_response(const uint8_t *data, size_t len, mc_host_wire_http_effect_response *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 16777216) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 39 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->failed = mc_host_wire_get_flag(&r);
        out->head = mc_host_wire_get_bytes(&r, 0);
        out->body = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_http_effect_response(const mc_host_wire_http_effect_response *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 39);
        mc_host_wire_put_u8(&w, 1);
            if (value->failed > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->failed);
            mc_host_wire_put_bytes(&w, value->head, 0);
            mc_host_wire_put_bytes(&w, value->body, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 16777216) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_WS_CONNECT_REQUEST_MAX_FRAME_BYTES ((size_t)UINT32_C(4194304))

typedef struct mc_host_wire_ws_connect_request {
    mc_host_wire_bytes url;
} mc_host_wire_ws_connect_request;
static inline mc_host_wire_result mc_host_wire_decode_ws_connect_request(const uint8_t *data, size_t len, mc_host_wire_ws_connect_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 4194304) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 40 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->url = mc_host_wire_get_bytes(&r, 1);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_ws_connect_request(const mc_host_wire_ws_connect_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 40);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->url, 1);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 4194304) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_WS_CONNECT_RESPONSE_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_ws_connect_response {
    uint8_t failed;
} mc_host_wire_ws_connect_response;
static inline mc_host_wire_result mc_host_wire_decode_ws_connect_response(const uint8_t *data, size_t len, mc_host_wire_ws_connect_response *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 41 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->failed = mc_host_wire_get_flag(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_ws_connect_response(const mc_host_wire_ws_connect_response *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 41);
        mc_host_wire_put_u8(&w, 1);
            if (value->failed > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->failed);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_WS_SEND_REQUEST_MAX_FRAME_BYTES ((size_t)UINT32_C(4194304))

typedef struct mc_host_wire_ws_send_request {
    mc_host_wire_bytes data;
} mc_host_wire_ws_send_request;
static inline mc_host_wire_result mc_host_wire_decode_ws_send_request(const uint8_t *data, size_t len, mc_host_wire_ws_send_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 4194304) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 42 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->data = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_ws_send_request(const mc_host_wire_ws_send_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 42);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->data, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 4194304) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_WS_SEND_RESPONSE_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_ws_send_response {
    uint8_t failed;
} mc_host_wire_ws_send_response;
static inline mc_host_wire_result mc_host_wire_decode_ws_send_response(const uint8_t *data, size_t len, mc_host_wire_ws_send_response *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 43 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->failed = mc_host_wire_get_flag(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_ws_send_response(const mc_host_wire_ws_send_response *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 43);
        mc_host_wire_put_u8(&w, 1);
            if (value->failed > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->failed);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_WS_CLOSE_EVENT_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_ws_close_event {
    uint32_t reason;
} mc_host_wire_ws_close_event;
static inline mc_host_wire_result mc_host_wire_decode_ws_close_event(const uint8_t *data, size_t len, mc_host_wire_ws_close_event *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 44 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->reason = mc_host_wire_get_u32(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_ws_close_event(const mc_host_wire_ws_close_event *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 44);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->reason);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_CATALOG_RECORD_MAX_FRAME_BYTES ((size_t)UINT32_C(18874368))

typedef struct mc_host_wire_catalog_record {
    mc_host_wire_bytes sha;
    mc_host_wire_bytes bytes;
} mc_host_wire_catalog_record;
static inline mc_host_wire_result mc_host_wire_decode_catalog_record(const uint8_t *data, size_t len, mc_host_wire_catalog_record *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 18874368) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 45 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->sha = mc_host_wire_get_bytes(&r, 1);
        out->bytes = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_catalog_record(const mc_host_wire_catalog_record *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 45);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->sha, 1);
            mc_host_wire_put_bytes(&w, value->bytes, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 18874368) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_CATALOG_APPLY_REQUEST_MAX_FRAME_BYTES ((size_t)UINT32_C(18874368))

typedef struct mc_host_wire_catalog_apply_request {
    mc_host_wire_bytes index_json;
    mc_host_wire_bytes base_digest;
    mc_host_wire_list records;
} mc_host_wire_catalog_apply_request;
static inline mc_host_wire_result mc_host_wire_decode_catalog_apply_request(const uint8_t *data, size_t len, mc_host_wire_catalog_apply_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 18874368) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 46 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->index_json = mc_host_wire_get_bytes(&r, 0);
        out->base_digest = mc_host_wire_get_bytes(&r, 1);
        {
        out->records.count = mc_host_wire_get_u32(&r);
        out->records.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->records.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_catalog_record nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_catalog_record(item.data, item.len, &nested);
        }
        out->records.len = r.off - list_start;
        }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_catalog_apply_request(const mc_host_wire_catalog_apply_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 46);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->index_json, 0);
            mc_host_wire_put_bytes(&w, value->base_digest, 1);
            {
            mc_host_wire_reader list_reader = {value->records.data, value->records.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->records.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_catalog_record nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_catalog_record(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->records.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->records.count);
            mc_host_wire_put(&w, value->records.data, value->records.len);
            }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 18874368) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_HOST_TOOL_ATTACHMENT_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_host_tool_attachment {
    mc_host_wire_bytes address;
    mc_host_wire_bytes description;
    mc_host_wire_bytes binding_name;
    mc_host_wire_bytes args_mode;
    uint8_t has_input_schema;
    mc_host_wire_bytes input_schema;
    uint8_t has_output_schema;
    mc_host_wire_bytes output_schema;
    uint8_t has_annotations;
    mc_host_wire_bytes annotations;
} mc_host_wire_host_tool_attachment;
static inline mc_host_wire_result mc_host_wire_decode_host_tool_attachment(const uint8_t *data, size_t len, mc_host_wire_host_tool_attachment *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 47 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->address = mc_host_wire_get_bytes(&r, 1);
        out->description = mc_host_wire_get_bytes(&r, 1);
        out->binding_name = mc_host_wire_get_bytes(&r, 1);
        out->args_mode = mc_host_wire_get_bytes(&r, 1);
    out->has_input_schema = mc_host_wire_get_flag(&r);
    if (out->has_input_schema) {
        out->input_schema = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_output_schema = mc_host_wire_get_flag(&r);
    if (out->has_output_schema) {
        out->output_schema = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_annotations = mc_host_wire_get_flag(&r);
    if (out->has_annotations) {
        out->annotations = mc_host_wire_get_bytes(&r, 1);
    }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_host_tool_attachment(const mc_host_wire_host_tool_attachment *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 47);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->address, 1);
            mc_host_wire_put_bytes(&w, value->description, 1);
            mc_host_wire_put_bytes(&w, value->binding_name, 1);
            mc_host_wire_put_bytes(&w, value->args_mode, 1);
        if (value->has_input_schema > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_input_schema);
        if (value->has_input_schema) {
            mc_host_wire_put_bytes(&w, value->input_schema, 1);
        }
        if (value->has_output_schema > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_output_schema);
        if (value->has_output_schema) {
            mc_host_wire_put_bytes(&w, value->output_schema, 1);
        }
        if (value->has_annotations > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_annotations);
        if (value->has_annotations) {
            mc_host_wire_put_bytes(&w, value->annotations, 1);
        }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_CONNECTION_ORIGIN_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_connection_origin {
    mc_host_wire_bytes value;
} mc_host_wire_connection_origin;
static inline mc_host_wire_result mc_host_wire_decode_connection_origin(const uint8_t *data, size_t len, mc_host_wire_connection_origin *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 54 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->value = mc_host_wire_get_bytes(&r, 1);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_connection_origin(const mc_host_wire_connection_origin *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 54);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->value, 1);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_CONNECTION_IDENTITY_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_connection_identity {
    mc_host_wire_bytes reference;
    mc_host_wire_list origins;
} mc_host_wire_connection_identity;
static inline mc_host_wire_result mc_host_wire_decode_connection_identity(const uint8_t *data, size_t len, mc_host_wire_connection_identity *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 53 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->reference = mc_host_wire_get_bytes(&r, 1);
        {
        out->origins.count = mc_host_wire_get_u32(&r);
        out->origins.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->origins.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_connection_origin nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_connection_origin(item.data, item.len, &nested);
        }
        out->origins.len = r.off - list_start;
        }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_connection_identity(const mc_host_wire_connection_identity *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 53);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->reference, 1);
            {
            mc_host_wire_reader list_reader = {value->origins.data, value->origins.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->origins.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_connection_origin nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_connection_origin(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->origins.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->origins.count);
            mc_host_wire_put(&w, value->origins.data, value->origins.len);
            }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_SNAPSHOT_ATTACHMENT_MANIFEST_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_snapshot_attachment_manifest {
    mc_host_wire_bytes content_digest;
    mc_host_wire_list routes;
    uint8_t has_persist_identity;
    mc_host_wire_bytes persist_identity;
    uint8_t has_http_identity;
    mc_host_wire_bytes http_identity;
    mc_host_wire_list connections;
    uint8_t has_policy_digest;
    mc_host_wire_bytes policy_digest;
    uint8_t native_net;
} mc_host_wire_snapshot_attachment_manifest;
static inline mc_host_wire_result mc_host_wire_decode_snapshot_attachment_manifest(const uint8_t *data, size_t len, mc_host_wire_snapshot_attachment_manifest *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 48 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 2 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->content_digest = mc_host_wire_get_bytes(&r, 0);
        {
        out->routes.count = mc_host_wire_get_u32(&r);
        out->routes.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->routes.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_host_call_attachment nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_host_call_attachment(item.data, item.len, &nested);
        }
        out->routes.len = r.off - list_start;
        }
    out->has_persist_identity = mc_host_wire_get_flag(&r);
    if (out->has_persist_identity) {
        out->persist_identity = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_http_identity = mc_host_wire_get_flag(&r);
    if (out->has_http_identity) {
        out->http_identity = mc_host_wire_get_bytes(&r, 1);
    }
        {
        out->connections.count = mc_host_wire_get_u32(&r);
        out->connections.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->connections.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_connection_identity nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_connection_identity(item.data, item.len, &nested);
        }
        out->connections.len = r.off - list_start;
        }
    out->has_policy_digest = mc_host_wire_get_flag(&r);
    if (out->has_policy_digest) {
        out->policy_digest = mc_host_wire_get_bytes(&r, 0);
    }
        out->native_net = mc_host_wire_get_flag(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_snapshot_attachment_manifest(const mc_host_wire_snapshot_attachment_manifest *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 48);
        mc_host_wire_put_u8(&w, 2);
            mc_host_wire_put_bytes(&w, value->content_digest, 0);
            {
            mc_host_wire_reader list_reader = {value->routes.data, value->routes.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->routes.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_host_call_attachment nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_host_call_attachment(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->routes.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->routes.count);
            mc_host_wire_put(&w, value->routes.data, value->routes.len);
            }
        if (value->has_persist_identity > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_persist_identity);
        if (value->has_persist_identity) {
            mc_host_wire_put_bytes(&w, value->persist_identity, 1);
        }
        if (value->has_http_identity > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_http_identity);
        if (value->has_http_identity) {
            mc_host_wire_put_bytes(&w, value->http_identity, 1);
        }
            {
            mc_host_wire_reader list_reader = {value->connections.data, value->connections.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->connections.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_connection_identity nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_connection_identity(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->connections.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->connections.count);
            mc_host_wire_put(&w, value->connections.data, value->connections.len);
            }
        if (value->has_policy_digest > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_policy_digest);
        if (value->has_policy_digest) {
            mc_host_wire_put_bytes(&w, value->policy_digest, 0);
        }
            if (value->native_net > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->native_net);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_CONNECTION_ATTACHMENT_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_connection_attachment {
    mc_host_wire_bytes reference;
    mc_host_wire_bytes credential_kind;
    mc_host_wire_bytes credential_a;
    mc_host_wire_bytes credential_b;
    mc_host_wire_list origins;
} mc_host_wire_connection_attachment;
static inline mc_host_wire_result mc_host_wire_decode_connection_attachment(const uint8_t *data, size_t len, mc_host_wire_connection_attachment *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 49 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->reference = mc_host_wire_get_bytes(&r, 1);
        out->credential_kind = mc_host_wire_get_bytes(&r, 1);
        out->credential_a = mc_host_wire_get_bytes(&r, 1);
        out->credential_b = mc_host_wire_get_bytes(&r, 1);
        {
        out->origins.count = mc_host_wire_get_u32(&r);
        out->origins.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->origins.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_connection_origin nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_connection_origin(item.data, item.len, &nested);
        }
        out->origins.len = r.off - list_start;
        }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_connection_attachment(const mc_host_wire_connection_attachment *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 49);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->reference, 1);
            mc_host_wire_put_bytes(&w, value->credential_kind, 1);
            mc_host_wire_put_bytes(&w, value->credential_a, 1);
            mc_host_wire_put_bytes(&w, value->credential_b, 1);
            {
            mc_host_wire_reader list_reader = {value->origins.data, value->origins.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->origins.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_connection_origin nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_connection_origin(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->origins.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->origins.count);
            mc_host_wire_put(&w, value->origins.data, value->origins.len);
            }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_CONNECTION_POLICY_ATTACHMENT_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_connection_policy_attachment {
    mc_host_wire_bytes owner;
    mc_host_wire_bytes pattern;
    mc_host_wire_bytes action;
} mc_host_wire_connection_policy_attachment;
static inline mc_host_wire_result mc_host_wire_decode_connection_policy_attachment(const uint8_t *data, size_t len, mc_host_wire_connection_policy_attachment *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 50 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->owner = mc_host_wire_get_bytes(&r, 1);
        out->pattern = mc_host_wire_get_bytes(&r, 1);
        out->action = mc_host_wire_get_bytes(&r, 1);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_connection_policy_attachment(const mc_host_wire_connection_policy_attachment *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 50);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->owner, 1);
            mc_host_wire_put_bytes(&w, value->pattern, 1);
            mc_host_wire_put_bytes(&w, value->action, 1);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_TOOL_APPROVAL_REQUEST_MAX_FRAME_BYTES ((size_t)UINT32_C(4194304))

typedef struct mc_host_wire_tool_approval_request {
    mc_host_wire_bytes connection;
    mc_host_wire_bytes method;
    mc_host_wire_bytes url;
    mc_host_wire_bytes origin;
    uint8_t has_args_digest;
    mc_host_wire_bytes args_digest;
} mc_host_wire_tool_approval_request;
static inline mc_host_wire_result mc_host_wire_decode_tool_approval_request(const uint8_t *data, size_t len, mc_host_wire_tool_approval_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 4194304) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 51 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->connection = mc_host_wire_get_bytes(&r, 1);
        out->method = mc_host_wire_get_bytes(&r, 1);
        out->url = mc_host_wire_get_bytes(&r, 1);
        out->origin = mc_host_wire_get_bytes(&r, 1);
    out->has_args_digest = mc_host_wire_get_flag(&r);
    if (out->has_args_digest) {
        out->args_digest = mc_host_wire_get_bytes(&r, 1);
    }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_tool_approval_request(const mc_host_wire_tool_approval_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 51);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->connection, 1);
            mc_host_wire_put_bytes(&w, value->method, 1);
            mc_host_wire_put_bytes(&w, value->url, 1);
            mc_host_wire_put_bytes(&w, value->origin, 1);
        if (value->has_args_digest > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_args_digest);
        if (value->has_args_digest) {
            mc_host_wire_put_bytes(&w, value->args_digest, 1);
        }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 4194304) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_TOOL_APPROVAL_RESPONSE_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_tool_approval_response {
    uint8_t allow;
    uint8_t remember_session;
} mc_host_wire_tool_approval_response;
static inline mc_host_wire_result mc_host_wire_decode_tool_approval_response(const uint8_t *data, size_t len, mc_host_wire_tool_approval_response *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 52 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->allow = mc_host_wire_get_flag(&r);
        out->remember_session = mc_host_wire_get_flag(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_tool_approval_response(const mc_host_wire_tool_approval_response *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 52);
        mc_host_wire_put_u8(&w, 1);
            if (value->allow > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->allow);
            if (value->remember_session > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->remember_session);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_LIVE_ATTACHMENT_MANIFEST_MAX_FRAME_BYTES ((size_t)UINT32_C(1048576))

typedef struct mc_host_wire_live_attachment_manifest {
    mc_host_wire_list routes;
    uint8_t has_persist_identity;
    mc_host_wire_bytes persist_identity;
    uint8_t has_http_identity;
    mc_host_wire_bytes http_identity;
    mc_host_wire_list connections;
    uint8_t has_policy_digest;
    mc_host_wire_bytes policy_digest;
    uint8_t native_net;
} mc_host_wire_live_attachment_manifest;
static inline mc_host_wire_result mc_host_wire_decode_live_attachment_manifest(const uint8_t *data, size_t len, mc_host_wire_live_attachment_manifest *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 1048576) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 55 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        {
        out->routes.count = mc_host_wire_get_u32(&r);
        out->routes.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->routes.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_host_call_attachment nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_host_call_attachment(item.data, item.len, &nested);
        }
        out->routes.len = r.off - list_start;
        }
    out->has_persist_identity = mc_host_wire_get_flag(&r);
    if (out->has_persist_identity) {
        out->persist_identity = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_http_identity = mc_host_wire_get_flag(&r);
    if (out->has_http_identity) {
        out->http_identity = mc_host_wire_get_bytes(&r, 1);
    }
        {
        out->connections.count = mc_host_wire_get_u32(&r);
        out->connections.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->connections.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_connection_identity nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_connection_identity(item.data, item.len, &nested);
        }
        out->connections.len = r.off - list_start;
        }
    out->has_policy_digest = mc_host_wire_get_flag(&r);
    if (out->has_policy_digest) {
        out->policy_digest = mc_host_wire_get_bytes(&r, 0);
    }
        out->native_net = mc_host_wire_get_flag(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_live_attachment_manifest(const mc_host_wire_live_attachment_manifest *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 55);
        mc_host_wire_put_u8(&w, 1);
            {
            mc_host_wire_reader list_reader = {value->routes.data, value->routes.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->routes.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_host_call_attachment nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_host_call_attachment(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->routes.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->routes.count);
            mc_host_wire_put(&w, value->routes.data, value->routes.len);
            }
        if (value->has_persist_identity > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_persist_identity);
        if (value->has_persist_identity) {
            mc_host_wire_put_bytes(&w, value->persist_identity, 1);
        }
        if (value->has_http_identity > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_http_identity);
        if (value->has_http_identity) {
            mc_host_wire_put_bytes(&w, value->http_identity, 1);
        }
            {
            mc_host_wire_reader list_reader = {value->connections.data, value->connections.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->connections.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_connection_identity nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_connection_identity(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->connections.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->connections.count);
            mc_host_wire_put(&w, value->connections.data, value->connections.len);
            }
        if (value->has_policy_digest > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_policy_digest);
        if (value->has_policy_digest) {
            mc_host_wire_put_bytes(&w, value->policy_digest, 0);
        }
            if (value->native_net > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->native_net);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 1048576) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}
#define MC_HOST_WIRE_EFFECT_TRANSCRIPT_ENTRY_MAX_FRAME_BYTES ((size_t)UINT32_C(16777472))

typedef struct mc_host_wire_effect_transcript_entry {
    int64_t sequence;
    uint32_t direction;
    uint32_t kind;
    int64_t event_id;
    uint8_t failed;
    mc_host_wire_bytes head;
    mc_host_wire_bytes body;
} mc_host_wire_effect_transcript_entry;
static inline mc_host_wire_result mc_host_wire_decode_effect_transcript_entry(const uint8_t *data, size_t len, mc_host_wire_effect_transcript_entry *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (len > 16777472) return MC_HOST_WIRE_TOO_LARGE;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 56 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->sequence = mc_host_wire_get_u64(&r);
        out->direction = mc_host_wire_get_u32(&r);
        out->kind = mc_host_wire_get_u32(&r);
        out->event_id = mc_host_wire_get_u64(&r);
        out->failed = mc_host_wire_get_flag(&r);
        out->head = mc_host_wire_get_bytes(&r, 0);
        out->body = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_effect_transcript_entry(const mc_host_wire_effect_transcript_entry *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 56);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u64(&w, (uint64_t)value->sequence);
            mc_host_wire_put_u32(&w, (uint32_t)value->direction);
            mc_host_wire_put_u32(&w, (uint32_t)value->kind);
            mc_host_wire_put_u64(&w, (uint64_t)value->event_id);
            if (value->failed > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->failed);
            mc_host_wire_put_bytes(&w, value->head, 0);
            mc_host_wire_put_bytes(&w, value->body, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (w.off > 16777472) return MC_HOST_WIRE_TOO_LARGE;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

#endif /* MC_EMBEDDER_CONTRACT_H */
