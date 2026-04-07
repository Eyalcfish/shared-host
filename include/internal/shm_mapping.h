#ifndef SHM_MAPPING_H
#define SHM_MAPPING_H

#include "shared_host.h"

#define OWN_ROLE_FLAG 1
#define SERVER_ROLE_FLAG 2
#define CLIENT_ROLE_FLAG 4
#define SHARED_HEADER_FLAG 8

sh_result_t sh_create_shared_memory(const char* port, const char* role, size_t size, char flags, shared_host_connection* out_connection);

sh_result_t sh_connect_to_shared_memory(const char* port, const char* role, size_t size, char flags, shared_host_connection* out_connection);

#ifdef _WIN32
sh_result_t sh_open_windows_event(const char* port, char flags, shared_host_connection* out_connection);
#endif

#endif /* SHM_MAPPING_H */