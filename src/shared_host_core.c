#include "internal/shm_mapping.h"
#include "internal/communication_model.h"
#include "shared_host.h"
#include <stdio.h>
#include <stdlib.h>


#ifdef _WIN32
    #include <windows.h>
#endif

sh_result_t create_shared_host_connection(const char* port, shared_host_connection* out_connection) {
    sh_result_t result = SH_OK;
    size_t size = 1 GB;

    HANDLE settingsBufferHandle = NULL;
    void* settingsBuffer = NULL;

    const char* settings_template = "Local\\shared_host_%s_settings";
    int settings_name_len = snprintf(NULL, 0, settings_template, port);
    char* settings_name = (char*)malloc(settings_name_len + 1);
    if (settings_name == NULL) {
        return SH_ERR_OOM;
    }
    sprintf(settings_name, settings_template, port);

    result = sh_create_shared_memory(settings_name, sizeof(shared_host_shared_settings_header)+sizeof(shared_host_shared_connection_header)*2, &settingsBufferHandle, &settingsBuffer);
    free(settings_name);

    if (result != SH_OK) {
        return result;
    }

    shared_host_shared_settings_header* settings_header = (shared_host_shared_settings_header*)settingsBuffer;
    settings_header->size = sizeof(shared_host_shared_settings_header)+sizeof(shared_host_shared_connection_header)*2;
    // settings_header->connection_type = ;

    HANDLE ownBufferHandle = NULL;
    void* ownBuffer = NULL;

    const char* own_buffer_template = "Local\\shared_host_%s_own_buffer";
    int own_buffer_name_len = snprintf(NULL, 0, own_buffer_template, port);
    char* own_buffer_name = (char*)malloc(own_buffer_name_len + 1);
    if (own_buffer_name == NULL) {
        UnmapViewOfFile(settingsBuffer);
        CloseHandle(settingsBufferHandle);
        return SH_ERR_OOM;
    }
    sprintf(own_buffer_name, own_buffer_template, port);

    result = sh_create_shared_memory(own_buffer_name, size, &ownBufferHandle, &ownBuffer);
    free(own_buffer_name);

    if (result != SH_OK) {
        UnmapViewOfFile(settingsBuffer);
        CloseHandle(settingsBufferHandle);
        return result;
    }

    HANDLE oppBufferHandle = NULL;
    void* oppBuffer = NULL;

    const char* opp_buffer_template = "Local\\shared_host_%s_opp_buffer";
    int opp_buffer_name_len = snprintf(NULL, 0, opp_buffer_template, port);
    char* opp_buffer_name = (char*)malloc(opp_buffer_name_len + 1);
    if (opp_buffer_name == NULL) {
        UnmapViewOfFile(settingsBuffer);
        CloseHandle(settingsBufferHandle);
        UnmapViewOfFile(ownBuffer);
        CloseHandle(ownBufferHandle);
        return SH_ERR_OOM;
    }
    sprintf(opp_buffer_name, opp_buffer_template, port);

    result = sh_create_shared_memory(opp_buffer_name, size, &oppBufferHandle, &oppBuffer);
    free(opp_buffer_name);

    if (result != SH_OK) {
        UnmapViewOfFile(settingsBuffer);
        CloseHandle(settingsBufferHandle);
        UnmapViewOfFile(ownBuffer);
        CloseHandle(ownBufferHandle);
        return result;
    }

    out_connection->shared_settings_page_handle = settingsBufferHandle;
    out_connection->own_shared_connection_buffer_handle = ownBufferHandle;
    out_connection->opp_shared_connection_buffer_handle = oppBufferHandle;

    out_connection->shared_settings_page_ptr = settingsBuffer;
    out_connection->own_shared_connection_header = (shared_host_shared_connection_header*)( ((char*) settingsBuffer) + sizeof(shared_host_shared_settings_header));
    out_connection->opp_shared_connection_header = out_connection->own_shared_connection_header+1;

    out_connection->own_shared_connection_header->page_start = ownBuffer;
    out_connection->own_shared_connection_header->page_end = ((char*)ownBuffer) + size;
    out_connection->opp_shared_connection_header->page_start = oppBuffer;
    out_connection->opp_shared_connection_header->page_end = ((char*)oppBuffer) + size;

    out_connection->own_shared_connection_header->current_item_ptr = ownBuffer;
    out_connection->own_shared_connection_header->last_item_ptr = ownBuffer;
    out_connection->opp_shared_connection_header->current_item_ptr = oppBuffer;
    out_connection->opp_shared_connection_header->last_item_ptr = oppBuffer;

    return SH_OK;
}

sh_result_t connect_to_shared_host_connection(const char* port, shared_host_connection** out_connection) {
    return SH_OK;
}

sh_result_t close_shared_host_connection(shared_host_connection* connection) {
    if (connection == NULL) {
        return SH_ERR_CONNECTION_CLOSED;
    }

    UnmapViewOfFile(connection->own_ptr);
    UnmapViewOfFile(connection->opp_ptr);
    UnmapViewOfFile(min(connection->own_header_shared_ptr, connection->opp_header_shared_ptr));
    CloseHandle(connection->ownSharedBufferHandle);
    CloseHandle(connection->oppSharedBufferHandle);
    CloseHandle(connection->ownEventHandle);
    CloseHandle(connection->oppEventHandle);
    CloseHandle(connection->sharedHeaderHandle);

    free(connection);

    return SH_OK;
}

sh_result_t write_to_shared_host_connection(shared_host_connection* connection, void* buffer, size_t buffer_size) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model_message* current_message_header = (communication_model_message*)connection->opp_current_message_ptr;

    // if (buffer_size > (size_t) atomic_load(&connection->opp_header_shared_ptr->left_space)) {
    //     printf("asASDALKSJKD KLASJKDLKASJDd %ld\n", atomic_load(&connection->opp_header_shared_ptr->left_space));
    //     return SH_ERR_MESSAGE_TOO_LONG;
    // }

    // i Wouldnt count this as the safest solution but it should work fine as long as the reader is fast enough, though it spins
    while (buffer_size > (size_t) atomic_load(&connection->opp_header_shared_ptr->left_space)) {
    }

    memcpy((char*)current_message_header + sizeof(communication_model_message), buffer, buffer_size);

    atomic_store(&current_message_header->message_size, buffer_size);
    atomic_store(&current_message_header->has_data, 1);
    connection->opp_current_message_ptr += sizeof(communication_model_message) + buffer_size;

    atomic_fetch_sub(&connection->opp_header_shared_ptr->left_space, sizeof(communication_model_message) + buffer_size);

    #ifdef _WIN32
    if (atomic_load(&connection->opp_header_shared_ptr->waiting_for_message) == 1) {
        SetEvent(connection->oppEventHandle);
    }
    #endif

    return SH_OK;
}

sh_result_t read_from_shared_host_connection(shared_host_connection* connection, void** buffer, size_t* buffer_size) { // ERROR HANDLING NEEDED
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model_message* current_message_header = (communication_model_message*)connection->own_current_message_ptr;

    #ifdef _WIN32
    if (atomic_load((&current_message_header->has_data)) == 0) {
        atomic_store(&connection->own_header_shared_ptr->waiting_for_message, 1);
        if (atomic_load((&current_message_header->has_data)) == 0) {
            WaitForSingleObject(connection->ownEventHandle, INFINITE);
        }
        atomic_store(&connection->own_header_shared_ptr->waiting_for_message, 0);
    }
    #endif

    atomic_long message_size = atomic_load(&current_message_header->message_size);
    *buffer = malloc(message_size);
    if (*buffer == NULL) {
        printf("[Reader] Failed to allocate memory for message. has_data: %d, message_size: %ld\n", atomic_load(&current_message_header->has_data), message_size);
        return SH_ERR_OOM;
    }

    memcpy(*buffer, (char*)current_message_header + sizeof(communication_model_message), message_size);
    *buffer_size = message_size;

    atomic_fetch_add(&connection->own_header_shared_ptr->left_space, sizeof(communication_model_message) + message_size);

    atomic_store((&current_message_header->has_data), 0);

    connection->own_current_message_ptr += sizeof(communication_model_message) + message_size;

    return SH_OK;
}
