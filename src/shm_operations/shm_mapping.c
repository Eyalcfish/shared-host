#include "shm_mapping.h"
#include "shared_host.h"

#ifdef _WIN32
    #include <windows.h>
#endif


sh_result_t sh_create_shared_memory(const char* port, size_t size, HANDLE* out_handle, void** ptr) {
    #ifdef _WIN32 
    if (size == 0) {
        return SH_ERR_INVALID_PARAMETER;
    }

    DWORD32 dwSizeLow = (DWORD32)(size & 0xFFFFFFFF);
    DWORD32 dwSizeHigh = (DWORD32)(size >> 32);

    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, dwSizeHigh, dwSizeLow, port);

    switch (GetLastError()) {
        case ERROR_ALREADY_EXISTS:
            return SH_ERR_PORT_IN_USE;
        case ERROR_INVALID_NAME:
            return SH_ERR_INVALID_PORT;
        case ERROR_NOT_ENOUGH_MEMORY:
            return SH_ERR_OOM;
        default:
            break;
    }

    void* ptr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    if (ptr == NULL) { // safety check
        return SH_ERR_UNKNOWN;
    }
    #endif

    return SH_OK;
}

sh_result_t sh_connect_to_shared_memory(const char* port, HANDLE* out_handle, void** ptr) {
    #ifdef _WIN32
        *out_handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, port);

        if (*out_handle == NULL) {
            switch (GetLastError()) {
                case ERROR_FILE_NOT_FOUND:
                    return SH_ERR_CONNECTION_CLOSED;
                case ERROR_INVALID_NAME:
                    return SH_ERR_INVALID_PORT;
                default:
                    return SH_ERR_UNKNOWN;
            }
        }

        *ptr = MapViewOfFile(*out_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    #endif

    return SH_OK;
}