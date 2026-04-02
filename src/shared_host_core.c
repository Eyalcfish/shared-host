#include <shared_host.h>
#include "shm_mapping.h"

#ifdef _WIN32
    #include <windows.h>
#endif

sh_result_t create_shared_host_connection(const char* port, size_t size, shared_host_connection** out_connection) {
    void *ptr = NULL;
    sh_result_t result = CreateSharedMemory(port, size, ptr);
    if (result != SH_OK) {
        return result;
    }

    
    return SH_OK;
}