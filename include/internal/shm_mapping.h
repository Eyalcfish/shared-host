#ifndef SHM_MAPPING_H
#define SHM_MAPPING_H

#include "shared_host.h"

sh_result_t sh_create_shared_memory(const char* port, size_t size, shared_host_connection* out_connection);

sh_result_t sh_connect_to_shared_memory(const char* port, shared_host_connection* out_connection);

#endif /* WINDOWS_MAPPING_H */