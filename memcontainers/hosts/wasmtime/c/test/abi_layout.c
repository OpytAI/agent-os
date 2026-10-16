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

int main(void) { return 0; }
