#ifndef SHM_MAPPING_H
#define SHM_MAPPING_H

#include "shared_host.h"

sh_result_t sh_create_shared_memory(const char* port, size_t size, HANDLE* bufferHandle, void** bufferPtr);

sh_result_t sh_connect_to_shared_memory(const char* port, size_t size, HANDLE* bufferHandle, void** bufferPtr);

sh_result_t create_windows_event(const char* event_name, HANDLE* event_handle);

sh_result_t connect_to_windows_event(const char* event_name, HANDLE* event_handle);

sh_result_t format_unique_name(char* port, char* category, size_t total_size, char** result);


// #ifdef _WIN32
// sh_result_t sh_open_windows_event(const char* port, shared_host_connection* out_connection);
// #endif

#endif /* SHM_MAPPING_H */
