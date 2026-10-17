#include "mc_host.h"

#include <stddef.h>

#define CHECK_LAYOUT(type_, name_) \
  _Static_assert(sizeof(type_) == MC_HOST_LAYOUT_##name_##_SIZE, #type_ " size"); \
  _Static_assert(_Alignof(type_) == MC_HOST_LAYOUT_##name_##_ALIGN, #type_ " align")
#define CHECK_FIELD(type_, name_, field_, constant_) \
  _Static_assert(offsetof(type_, field_) == MC_HOST_LAYOUT_##name_##_##constant_##_OFFSET, #type_ "." #field_ " offset")

CHECK_LAYOUT(mc_host_header_v1, HEADER);
CHECK_FIELD(mc_host_header_v1, HEADER, size, SIZE);
CHECK_FIELD(mc_host_header_v1, HEADER, version, VERSION);
CHECK_FIELD(mc_host_header_v1, HEADER, flags, FLAGS);

CHECK_LAYOUT(mc_host_buffer_v1, BUFFER);
CHECK_FIELD(mc_host_buffer_v1, BUFFER, data, DATA);
CHECK_FIELD(mc_host_buffer_v1, BUFFER, capacity, CAPACITY);
CHECK_FIELD(mc_host_buffer_v1, BUFFER, required, REQUIRED);

CHECK_LAYOUT(mc_host_error_v1, ERROR);
CHECK_FIELD(mc_host_error_v1, ERROR, header, HEADER);
CHECK_FIELD(mc_host_error_v1, ERROR, domain, DOMAIN);
CHECK_FIELD(mc_host_error_v1, ERROR, code, CODE);
CHECK_FIELD(mc_host_error_v1, ERROR, retryable, RETRYABLE);
CHECK_FIELD(mc_host_error_v1, ERROR, reserved, RESERVED);
CHECK_FIELD(mc_host_error_v1, ERROR, message, MESSAGE);

CHECK_LAYOUT(mc_host_runtime_options_v1, RUNTIME_OPTIONS);
CHECK_FIELD(mc_host_runtime_options_v1, RUNTIME_OPTIONS, header, HEADER);

CHECK_LAYOUT(mc_host_library_info_v1, LIBRARY_INFO);
CHECK_FIELD(mc_host_library_info_v1, LIBRARY_INFO, header, HEADER);
CHECK_FIELD(mc_host_library_info_v1, LIBRARY_INFO, abi_major, ABI_MAJOR);
CHECK_FIELD(mc_host_library_info_v1, LIBRARY_INFO, abi_minor, ABI_MINOR);
CHECK_FIELD(mc_host_library_info_v1, LIBRARY_INFO, implementation_table_size, IMPLEMENTATION_TABLE_SIZE);
CHECK_FIELD(mc_host_library_info_v1, LIBRARY_INFO, feature_bits, FEATURE_BITS);

CHECK_LAYOUT(mc_host_api_v1, API);
CHECK_FIELD(mc_host_api_v1, API, header, HEADER);
CHECK_FIELD(mc_host_api_v1, API, abi_major, ABI_MAJOR);
CHECK_FIELD(mc_host_api_v1, API, abi_minor, ABI_MINOR);
CHECK_FIELD(mc_host_api_v1, API, implementation_table_size, IMPLEMENTATION_TABLE_SIZE);
CHECK_FIELD(mc_host_api_v1, API, feature_bits, FEATURE_BITS);
CHECK_FIELD(mc_host_api_v1, API, runtime_create, RUNTIME_CREATE);
CHECK_FIELD(mc_host_api_v1, API, runtime_close, RUNTIME_CLOSE);
CHECK_FIELD(mc_host_api_v1, API, library_info, LIBRARY_INFO);
CHECK_FIELD(mc_host_api_v1, API, get_interface, GET_INTERFACE);
CHECK_LAYOUT(mc_host_bytes_v1, BYTES);
CHECK_FIELD(mc_host_bytes_v1, BYTES, data, DATA);
CHECK_FIELD(mc_host_bytes_v1, BYTES, len, LEN);

CHECK_LAYOUT(mc_host_kernel_info_v1, KERNEL_INFO);
CHECK_FIELD(mc_host_kernel_info_v1, KERNEL_INFO, header, HEADER);
CHECK_FIELD(mc_host_kernel_info_v1, KERNEL_INFO, digest, DIGEST);

CHECK_LAYOUT(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, header, HEADER);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, layers, LAYERS);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, layer_count, LAYER_COUNT);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, contract_tier, CONTRACT_TIER);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, contract_budget_mib, CONTRACT_BUDGET_MIB);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, contract_fuel, CONTRACT_FUEL);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, requested_workers, REQUESTED_WORKERS);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, flags, FLAGS);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, terminal_cols, TERMINAL_COLS);
CHECK_FIELD(mc_host_vm_create_options_v1, VM_CREATE_OPTIONS, terminal_rows, TERMINAL_ROWS);

CHECK_LAYOUT(mc_host_vm_restore_options_v1, VM_RESTORE_OPTIONS);
CHECK_FIELD(mc_host_vm_restore_options_v1, VM_RESTORE_OPTIONS, header, HEADER);
CHECK_FIELD(mc_host_vm_restore_options_v1, VM_RESTORE_OPTIONS, snapshot, SNAPSHOT);
CHECK_FIELD(mc_host_vm_restore_options_v1, VM_RESTORE_OPTIONS, baseline, BASELINE);
CHECK_FIELD(mc_host_vm_restore_options_v1, VM_RESTORE_OPTIONS, mode, MODE);
CHECK_FIELD(mc_host_vm_restore_options_v1, VM_RESTORE_OPTIONS, reserved, RESERVED);

CHECK_LAYOUT(mc_host_drive_options_v1, DRIVE_OPTIONS);
CHECK_FIELD(mc_host_drive_options_v1, DRIVE_OPTIONS, header, HEADER);
CHECK_FIELD(mc_host_drive_options_v1, DRIVE_OPTIONS, max_ticks, MAX_TICKS);
CHECK_FIELD(mc_host_drive_options_v1, DRIVE_OPTIONS, reserved, RESERVED);
CHECK_FIELD(mc_host_drive_options_v1, DRIVE_OPTIONS, stop_mask, STOP_MASK);

CHECK_LAYOUT(mc_host_vm_status_v1, VM_STATUS);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, header, HEADER);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, state, STATE);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, last_tick, LAST_TICK);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, flags, FLAGS);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, exit_code, EXIT_CODE);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, output_records, OUTPUT_RECORDS);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, output_bytes, OUTPUT_BYTES);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, output_dropped_bytes, OUTPUT_DROPPED_BYTES);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, inflight_egress, INFLIGHT_EGRESS);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, pending_commits, PENDING_COMMITS);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, workers, WORKERS);
CHECK_FIELD(mc_host_vm_status_v1, VM_STATUS, reserved, RESERVED);

CHECK_LAYOUT(mc_host_drive_result_v1, DRIVE_RESULT);
CHECK_FIELD(mc_host_drive_result_v1, DRIVE_RESULT, header, HEADER);
CHECK_FIELD(mc_host_drive_result_v1, DRIVE_RESULT, ticks, TICKS);
CHECK_FIELD(mc_host_drive_result_v1, DRIVE_RESULT, stop_reason, STOP_REASON);
CHECK_FIELD(mc_host_drive_result_v1, DRIVE_RESULT, status, STATUS);

CHECK_LAYOUT(mc_host_vm_api_v1, VM_API);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, header, HEADER);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, kernel_load, KERNEL_LOAD);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, kernel_close, KERNEL_CLOSE);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, vm_create, VM_CREATE);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, vm_restore, VM_RESTORE);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, vm_status, VM_STATUS);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, vm_drive, VM_DRIVE);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, vm_close, VM_CLOSE);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, vm_cancel_and_close, VM_CANCEL_AND_CLOSE);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, terminal_input, TERMINAL_INPUT);
CHECK_FIELD(mc_host_vm_api_v1, VM_API, terminal_resize, TERMINAL_RESIZE);
CHECK_LAYOUT(mc_host_output_record_v1, OUTPUT_RECORD);
CHECK_FIELD(mc_host_output_record_v1, OUTPUT_RECORD, header, HEADER);
CHECK_FIELD(mc_host_output_record_v1, OUTPUT_RECORD, kind, KIND);
CHECK_FIELD(mc_host_output_record_v1, OUTPUT_RECORD, channel, CHANNEL);
CHECK_FIELD(mc_host_output_record_v1, OUTPUT_RECORD, sequence, SEQUENCE);
CHECK_FIELD(mc_host_output_record_v1, OUTPUT_RECORD, byte_offset, BYTE_OFFSET);
CHECK_FIELD(mc_host_output_record_v1, OUTPUT_RECORD, length, LENGTH);
CHECK_FIELD(mc_host_output_record_v1, OUTPUT_RECORD, first_lost_sequence, FIRST_LOST_SEQUENCE);
CHECK_FIELD(mc_host_output_record_v1, OUTPUT_RECORD, last_lost_sequence, LAST_LOST_SEQUENCE);
CHECK_FIELD(mc_host_output_record_v1, OUTPUT_RECORD, byte_end, BYTE_END);

CHECK_LAYOUT(mc_host_output_stats_v1, OUTPUT_STATS);
CHECK_FIELD(mc_host_output_stats_v1, OUTPUT_STATS, header, HEADER);
CHECK_FIELD(mc_host_output_stats_v1, OUTPUT_STATS, records, RECORDS);
CHECK_FIELD(mc_host_output_stats_v1, OUTPUT_STATS, retained_bytes, RETAINED_BYTES);
CHECK_FIELD(mc_host_output_stats_v1, OUTPUT_STATS, dropped_bytes, DROPPED_BYTES);
CHECK_FIELD(mc_host_output_stats_v1, OUTPUT_STATS, leased_records, LEASED_RECORDS);
CHECK_FIELD(mc_host_output_stats_v1, OUTPUT_STATS, flags, FLAGS);
CHECK_FIELD(mc_host_output_stats_v1, OUTPUT_STATS, reserved, RESERVED);

CHECK_LAYOUT(mc_host_output_api_v1, OUTPUT_API);
CHECK_FIELD(mc_host_output_api_v1, OUTPUT_API, header, HEADER);
CHECK_FIELD(mc_host_output_api_v1, OUTPUT_API, next, NEXT);
CHECK_FIELD(mc_host_output_api_v1, OUTPUT_API, read, READ);
CHECK_FIELD(mc_host_output_api_v1, OUTPUT_API, ack, ACK);
CHECK_FIELD(mc_host_output_api_v1, OUTPUT_API, release, RELEASE);
CHECK_FIELD(mc_host_output_api_v1, OUTPUT_API, stats, STATS);



int main(void) { return 0; }
