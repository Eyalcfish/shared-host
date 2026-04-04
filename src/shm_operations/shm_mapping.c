#include "internal/shm_mapping.h"
#include "shared_host.h"
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
#endif


sh_result_t sh_create_shared_memory(const char* port, size_t size, shared_host_connection* out_connection) {
    #ifdef _WIN32 
    if (size == 0) {
        return SH_ERR_INVALID_PARAMETER;
    }

    char *name = NULL;
    size_t name_len = snprintf(NULL, 0, "Local\\shared-host_%s", port) + 1;
    name = (char*)malloc(name_len);
    if (name == NULL) {
        return SH_ERR_OOM;
    }

    snprintf(name, name_len, "Local\\shared-host_%s", port);

    DWORD32 dwSizeLow = (DWORD32)(size & 0xFFFFFFFF);
    DWORD32 dwSizeHigh = (DWORD32)(size >> 32);

    out_connection->sharedBufferHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, dwSizeHigh, dwSizeLow, name);
    
    if (out_connection->sharedBufferHandle == NULL) {
        int error = GetLastError();
         free(name);
        
        switch (error) {
            case ERROR_ALREADY_EXISTS:
                return SH_ERR_PORT_IN_USE;
            case ERROR_INVALID_NAME:
                return SH_ERR_INVALID_PORT;
            case ERROR_NOT_ENOUGH_MEMORY:
                return SH_ERR_OOM;
            case ERROR_INVALID_PARAMETER:
                return SH_ERR_INVALID_PARAMETER;
            default:
                return SH_ERR_UNKNOWN;
        }
    }
    free(name);
    

    out_connection->ptr = MapViewOfFile(out_connection->sharedBufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    if (out_connection->ptr == NULL) { // maybe unncessery safety check
        switch (GetLastError()) {
            case ERROR_INVALID_HANDLE:
                return SH_ERR_CONNECTION_CLOSED;
            default:
                return SH_ERR_UNKNOWN;
        }
    }
    #endif

    return SH_OK;
}

sh_result_t sh_connect_to_shared_memory(const char* port, shared_host_connection* out_connection) {
    #ifdef _WIN32
    char *name = NULL;
    size_t name_len = snprintf(NULL, 0, "Local\\shared-host_%s", port) + 1;
    name = (char*)malloc(name_len);
    if (name == NULL) {
        return SH_ERR_OOM;
    }

    snprintf(name, name_len, "Local\\shared-host_%s", port);

    out_connection->sharedBufferHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name);

    if (out_connection->sharedBufferHandle == NULL) {
        int error = GetLastError();
         free(name);
        
        switch (error) {
            case ERROR_ALREADY_EXISTS:
            return SH_ERR_PORT_IN_USE;
            case ERROR_INVALID_NAME:
                return SH_ERR_INVALID_PORT;
            case ERROR_NOT_ENOUGH_MEMORY:
            return SH_ERR_OOM;
            default:
            return SH_ERR_UNKNOWN;
        }
    }
    free(name);

    out_connection->ptr = MapViewOfFile(out_connection->sharedBufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    #endif

    return SH_OK;
}

#ifdef _WIN32
sh_result_t sh_open_windows_event(const char* port, shared_host_connection* out_connection) {
    char *name = NULL;
    size_t name_len = snprintf(NULL, 0, "Local\\shared-host_event_%s", port) + 1;
    name = (char*)malloc(name_len);
    if (name == NULL) {
        return SH_ERR_OOM;
    }

    snprintf(name, name_len, "Local\\shared-host_event_%s", port);

    HANDLE eventHandle = CreateEvent(NULL,FALSE,FALSE,name);

    int error = GetLastError();
    free(name);

    if (eventHandle == NULL) {
        switch (error) {
            case ERROR_ALREADY_EXISTS:
                return SH_ERR_PORT_IN_USE;
            case ERROR_INVALID_PARAMETER:
                return SH_ERR_INVALID_PORT;
            case ERROR_FILENAME_EXCED_RANGE:
                return SH_ERR_INVALID_PARAMETER;
            case ERROR_NOT_ENOUGH_MEMORY:
            case ERROR_OUTOFMEMORY:
                return SH_ERR_OOM;
            default:
                return SH_ERR_UNKNOWN;
        }
    }

    out_connection->eventHandle = eventHandle; 
    
    return SH_OK;
}
#endif