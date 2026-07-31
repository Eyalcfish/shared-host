#include "internal/shm_mapping.h"
#include "shared_host.h"
#include <dlgs.h>
#include <errhandlingapi.h>
#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>
    #include <memoryapi.h>
#endif

sh_result_t sh_create_shared_memory(const char* port, size_t size, HANDLE* bufferHandle, void** bufferPtr) {
    if (size == 0) {
        return SH_ERR_INVALID_PARAMETER;
    }

    DWORD32 Lower32 = (DWORD32)(size & 0xFFFFFFFF);
    DWORD32 Upper32 = (DWORD32)((uint64_t)size >> 32);

    *bufferHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, Upper32, Lower32, port);

    if (*bufferHandle == NULL) {
        int error = GetLastError();

        switch (error) {
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

    if (GetLastError() == ERROR_ALREADY_EXISTS) { // ERROR_ALREADY_EXISTS can occur even of the handle is not NULL
        CloseHandle(*bufferHandle);
        return SH_ERR_PORT_IN_USE;
    }


    *bufferPtr = MapViewOfFile(*bufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (*bufferPtr == NULL) {
        CloseHandle(*bufferHandle);
        return SH_ERR_OOM;
    }

    return SH_OK;
}

sh_result_t sh_connect_to_shared_memory(const char* port, size_t size, HANDLE* bufferHandle, void** bufferPtr) {

    *bufferHandle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, port);

    if (*bufferHandle == NULL) {
        int error = GetLastError();

        switch (error) {
            case ERROR_FILE_NOT_FOUND:
                return SH_ERR_CONNECTION_CLOSED;
            case ERROR_INVALID_NAME:
                return SH_ERR_INVALID_PORT;
            case ERROR_NOT_ENOUGH_MEMORY:
                return SH_ERR_OOM;
            default:
                return SH_ERR_UNKNOWN;
        }
    }

    *bufferPtr = MapViewOfFile(*bufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (*bufferPtr == NULL) {
        CloseHandle(*bufferHandle);
        return SH_ERR_OOM;
    }

    return SH_OK;
}

sh_result_t create_windows_event(const char* event_name, HANDLE* event_handle) {
    printf("creating event: %s\n", event_name);
    *event_handle = CreateEventA(NULL, TRUE, FALSE, event_name);
    printf("event handle: %p\n", *event_handle);

    if (*event_handle == NULL) {
        return SH_ERR_OOM;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return SH_ERR_PORT_IN_USE;
    }

    return SH_OK;
}

sh_result_t connect_to_windows_event(const char* event_name, HANDLE* event_handle) {
    *event_handle = OpenEventA(EVENT_ALL_ACCESS, FALSE, event_name);

    if (*event_handle == NULL) {
        return SH_ERR_OOM;
    }

    return SH_OK;
}

sh_result_t format_unique_name(char* port, char* category, size_t total_size, char** result) {
    char* full_name = (char*)malloc(total_size + 21);

    if (full_name == NULL) {
        return SH_ERR_OOM;
    }

    snprintf(full_name, total_size+21, "Local\\shared_host_%s_%s", port, category);
    *result = full_name;
    return SH_OK;
}
