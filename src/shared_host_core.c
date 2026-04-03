#include <shared_host.h>
#include "shm_mapping.h"

#ifdef _WIN32
    #include <windows.h>
#endif

sh_result_t create_shared_host_connection(const char* port, size_t size, shared_host_connection** out_connection) {
    *out_connection = (shared_host_connection*)malloc(sizeof(shared_host_connection));

    if (*out_connection == NULL) {
        return SH_ERR_OOM;
    }
    void *ptr = NULL;
    sh_result_t result = CreateSharedMemory(port, size, ptr);
    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return result;
    }

    (*out_connection)->ptr = ptr;
    (*out_connection)->size = size;
    (*out_connection)->port = port;
    
    return SH_OK;
}