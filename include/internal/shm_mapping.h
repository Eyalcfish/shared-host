#ifndef SHM_MAPPING_H
#define SHM_MAPPING_H

#include "shared_host.h"

sh_result_t sh_create_shared_memory(const char* port, size_t size, HANDLE* out_handle, void** ptr);

sh_result_t sh_connect_to_shared_memory(const char* port, HANDLE* out_handle, void** ptr);

#endif /* WINDOWS_MAPPING_H */