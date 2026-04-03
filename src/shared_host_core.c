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

    HANDLE handle = NULL;
    void *ptr = NULL;
    sh_result_t result = sh_create_shared_memory(port, size, &handle, &ptr);
    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return result;
    }

    (*out_connection)->ptr = ptr;
    (*out_connection)->size = size;
    (*out_connection)->port = port;
    (*out_connection)->handle = handle;

    *(size_t*)ptr = size;

    return SH_OK;
}

sh_result_t connect_to_shared_host_connection(const char* port, shared_host_connection** out_connection) {
    *out_connection = (shared_host_connection*)malloc(sizeof(shared_host_connection));

    if (*out_connection == NULL) {
        return SH_ERR_OOM;
    }
    
    HANDLE handle = NULL;
    void *ptr = NULL;

    sh_result_t result = sh_connect_to_shared_memory(port, &handle, &ptr);

    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return SH_ERR_CONNECTION_CLOSED; 
    }

    (*out_connection)->ptr = ptr;
    (*out_connection)->size = *(size_t*)ptr;
    (*out_connection)->port = port;
    (*out_connection)->handle = handle;

    return SH_OK;
}