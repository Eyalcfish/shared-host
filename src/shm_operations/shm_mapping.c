#include "internal/shm_mapping.h"
#include "shared_host.h"
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #include <memoryapi.h>
#endif


sh_result_t sh_create_shared_memory(const char* port, const char* role, size_t size, char flags, shared_host_connection* out_connection) {
    #ifdef _WIN32 
    if (size == 0) {
        return SH_ERR_INVALID_PARAMETER;
    }

    char *name = NULL;
    size_t name_len;
    if (flags & SHARED_HEADER_FLAG) {
        name_len = snprintf(NULL, 0, "Local\\shared-host_%s_header", port) + 1;
    } else {
        name_len = snprintf(NULL, 0, "Local\\shared-host_%s_%s", port, role) + 1;
    }
    name = (char*)malloc(name_len);
    if (name == NULL) {
        return SH_ERR_OOM;
    }

    if (flags & SHARED_HEADER_FLAG) {
        snprintf(name, name_len, "Local\\shared-host_%s_header", port);
    } else {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        size_t granularity = si.dwAllocationGranularity;
    
        size = (size + granularity - 1) & ~(granularity - 1);
        snprintf(name, name_len, "Local\\shared-host_%s_%s", port, role);
    }


    DWORD32 dwSizeLow = (DWORD32)(size & 0xFFFFFFFF);
    DWORD32 dwSizeHigh = (DWORD32)(size >> 32);

    HANDLE bufferHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, dwSizeHigh, dwSizeLow, name);
    
    if (bufferHandle == NULL) {
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

    void* ptr;
    if (!(flags & SHARED_HEADER_FLAG)) {
        
        ptr = VirtualAlloc(NULL, 2*size, MEM_RESERVE, PAGE_NOACCESS);
        if (ptr == NULL) {
            CloseHandle(bufferHandle);
            return SH_ERR_OOM;
        }

        VirtualFree(ptr, 0, MEM_RELEASE);

        PVOID map1 = MapViewOfFileEx(bufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, size, ptr);
        PVOID map2 = MapViewOfFileEx(bufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, size, (char*)ptr + size);

        if (map1 == NULL) {
            // Handle error
            CloseHandle(bufferHandle);

            return SH_ERR_OOM;
        }

        if (map2 == NULL) {
            // Handle error
            UnmapViewOfFile(map1);
            CloseHandle(bufferHandle);
            return SH_ERR_OOM;
        }
    } else {
        ptr = MapViewOfFile(bufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    }
    
    
    if (flags & SHARED_HEADER_FLAG) {
        out_connection->own_header_shared_ptr = ptr;
        out_connection->opp_header_shared_ptr = ptr + sizeof(communication_model);
    } else if (strcmp(role, "server") == 0) { // can prob branchless this
        out_connection->ownSharedBufferHandle = bufferHandle;
        out_connection->own_ptr = ptr;
    } else {
        out_connection->oppSharedBufferHandle = bufferHandle;
        out_connection->opp_ptr = ptr;
    }
    #endif

    return SH_OK;
}

sh_result_t sh_connect_to_shared_memory(const char* port, const char* role, size_t size, char flags, shared_host_connection* out_connection) {
    #ifdef _WIN32
    char *name = NULL;
    size_t name_len;
    if (flags & SHARED_HEADER_FLAG) {
        name_len = snprintf(NULL, 0, "Local\\shared-host_%s_header", port) + 1;
    } else {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        size_t granularity = si.dwAllocationGranularity;
    
        size = (size + granularity - 1) & ~(granularity - 1);
        
        name_len = snprintf(NULL, 0, "Local\\shared-host_%s_%s", port, role) + 1;
    }
    name = (char*)malloc(name_len);
    if (name == NULL) {
        return SH_ERR_OOM;
    }

    if (flags & SHARED_HEADER_FLAG) {
        snprintf(name, name_len, "Local\\shared-host_%s_header", port);
    } else {
        snprintf(name, name_len, "Local\\shared-host_%s_%s", port, role);
    }

    HANDLE bufferHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name);

    if (bufferHandle == NULL) {
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

    if (flags & SHARED_HEADER_FLAG) {
        void* ptr = MapViewOfFile(bufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        out_connection->sharedHeaderHandle = bufferHandle;
        out_connection->own_header_shared_ptr = ptr + sizeof(communication_model);
        out_connection->opp_header_shared_ptr = ptr;
    } else { 
        void* ptr = VirtualAlloc(NULL, 2*size, MEM_RESERVE, PAGE_NOACCESS);
        if (ptr == NULL) {
            CloseHandle(bufferHandle);
            return SH_ERR_OOM;
        }

        VirtualFree(ptr, 0, MEM_RELEASE);

        PVOID map1 = MapViewOfFileEx(bufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, size, ptr);
        PVOID map2 = MapViewOfFileEx(bufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, size, (char*)ptr + size);

        if (map1 == NULL) {
            CloseHandle(bufferHandle);

            return SH_ERR_OOM;
        }

        if (map2 == NULL) {
            UnmapViewOfFile(map1);
            CloseHandle(bufferHandle);
            return SH_ERR_OOM;
        }
        if (strcmp(role, "server") == 0) { // can prob branchless this
            out_connection->oppSharedBufferHandle = bufferHandle;
            out_connection->opp_ptr = ptr;
        } else {
            out_connection->ownSharedBufferHandle = bufferHandle;
            out_connection->own_ptr = ptr;
        }
    }

    #endif

    return SH_OK;
}

#ifdef _WIN32
sh_result_t sh_open_windows_event(const char* port, char flags, shared_host_connection* out_connection) {
    char *name = NULL;
    size_t name_len;
    if (flags & SERVER_ROLE_FLAG) {
        name_len = snprintf(NULL, 0, "Local\\shared-host_event_%s_server", port) + 1;
    } else {
        name_len = snprintf(NULL, 0, "Local\\shared-host_event_%s_client", port) + 1;
    }
    name = (char*)malloc(name_len);
    if (name == NULL) {
        return SH_ERR_OOM;
    }

    if (flags & SERVER_ROLE_FLAG) {
        snprintf(name, name_len, "Local\\shared-host_event_%s_server", port);
    } else {
        snprintf(name, name_len, "Local\\shared-host_event_%s_client", port);
    }

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

    if (flags & OWN_ROLE_FLAG) {
        out_connection->ownEventHandle = eventHandle;
    } else {
        out_connection->oppEventHandle = eventHandle;
    }

    return SH_OK;
}
#endif