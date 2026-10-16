#ifndef MC_HOST_H
#define MC_HOST_H

#include <stdint.h>
#include "memcontainers/contracts/embedder.gen.h"

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

typedef struct mc_host_error_v1 {
  mc_host_header_v1 header;
  uint32_t domain;
  int32_t code;
  uint8_t retryable;
  uint8_t reserved[7];
  mc_host_buffer_v1 message;
} mc_host_error_v1;

typedef struct mc_host_runtime_options_v1 {
  mc_host_header_v1 header;
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
