#include "internal/shm_mapping.h"
#include "internal/communication_model.h"
#include "shared_host.h"
#include <stdio.h>


#ifdef _WIN32
    #include <windows.h>
#endif

sh_result_t create_shared_host_connection(const char* port, shared_host_connection** out_connection) {
    *out_connection = (shared_host_connection*)malloc(sizeof(shared_host_connection));

    size_t size = 1000000000; // default size, need to make this dynamic later

    if (*out_connection == NULL) {
        return SH_ERR_OOM;
    }

    sh_result_t result = sh_create_shared_memory(port, "server", size, 0, *out_connection);

    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return result;
    }

    
    result = sh_create_shared_memory(port, "client", size, 0, *out_connection);

    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return result;
    }

    result = sh_create_shared_memory(port, "header", 100, SHARED_HEADER_FLAG, *out_connection);

    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return result;
    }

    #ifdef _WIN32
    result = sh_open_windows_event(port, *out_connection);
    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return result;
    }
    #endif

    (*out_connection)->size = size;
    (*out_connection)->port = port;

    (*out_connection)->own_header_shared_ptr->capacity = size;
    (*out_connection)->opp_header_shared_ptr->capacity = size;

    (*out_connection)->own_header_shared_ptr->left_space = size;
    (*out_connection)->opp_header_shared_ptr->left_space = size;

    (*out_connection)->own_header_shared_ptr->messages_unread = 0;// already initalizes as 0, so kinda unnecessery but for readability
    (*out_connection)->opp_header_shared_ptr->messages_unread = 0;

    (*out_connection)->own_current_message_ptr = (*out_connection)->own_ptr;
    (*out_connection)->opp_current_message_ptr = (*out_connection)->opp_ptr;

    return SH_OK;
}

sh_result_t connect_to_shared_host_connection(const char* port, shared_host_connection** out_connection) {
    *out_connection = (shared_host_connection*)malloc(sizeof(shared_host_connection));

    if (*out_connection == NULL) {
        return SH_ERR_OOM;
    }

    sh_result_t result = sh_connect_to_shared_memory(port, "header", 0, SHARED_HEADER_FLAG, *out_connection);

    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return SH_ERR_CONNECTION_CLOSED;  
    }

    (*out_connection)->size = ((*out_connection)->own_header_shared_ptr)->capacity; // one of those buffers need to be chosen for saving size

    result = sh_connect_to_shared_memory(port, "client", (*out_connection)->size, 0, *out_connection);

    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return SH_ERR_CONNECTION_CLOSED;  
    }

    result = sh_connect_to_shared_memory(port, "server", (*out_connection)->size, 0, *out_connection);

    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return SH_ERR_CONNECTION_CLOSED;
    }


    #ifdef _WIN32
    result = sh_open_windows_event(port, *out_connection);
    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return result;
    }
    #endif

    (*out_connection)->own_current_message_ptr = (*out_connection)->own_ptr;
    (*out_connection)->opp_current_message_ptr = (*out_connection)->opp_ptr;
    (*out_connection)->port = port;

    return SH_OK;
}

sh_result_t close_shared_host_connection(shared_host_connection* connection) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    #ifdef _WIN32
        UnmapViewOfFile(connection->own_ptr);
        UnmapViewOfFile(connection->opp_ptr);
        UnmapViewOfFile(min(connection->own_header_shared_ptr, connection->opp_header_shared_ptr)); 
        CloseHandle(connection->ownSharedBufferHandle);
        CloseHandle(connection->oppSharedBufferHandle);
        CloseHandle(connection->eventHandle);
        CloseHandle(connection->sharedHeaderHandle);
    #endif

    free(connection);

    return SH_OK;
}

sh_result_t write_to_shared_host_connection(shared_host_connection* connection, void* buffer, size_t buffer_size) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model_message* current_message_header = (communication_model_message*)connection->opp_current_message_ptr;

    if (buffer_size > (size_t) atomic_load(&connection->opp_header_shared_ptr->left_space)) {
        printf("asASDALKSJKD KLASJKDLKASJDd %ld\n", atomic_load(&connection->opp_header_shared_ptr->left_space));
        return SH_ERR_MESSAGE_TOO_LONG;
    }
    
    memcpy(current_message_header + sizeof(communication_model_message), buffer, buffer_size);

    current_message_header->message_size = buffer_size;

    atomic_fetch_sub(&connection->opp_header_shared_ptr->left_space, sizeof(communication_model_message) + buffer_size);
    atomic_fetch_add(&connection->opp_header_shared_ptr->messages_unread, 1);
    atomic_store(&current_message_header->has_data, 1);
    connection->opp_current_message_ptr += sizeof(communication_model_message) + buffer_size;
    
    #ifdef _WIN32
    if (atomic_load(&connection->opp_header_shared_ptr->messages_unread) == 1) { // if this is the first unread message, signal the reader
        SetEvent(connection->eventHandle);
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
    if (atomic_load((&connection->own_header_shared_ptr->messages_unread)) == 0) {
        WaitForSingleObject(connection->eventHandle, INFINITE);
    } else {
        atomic_fetch_sub(&connection->own_header_shared_ptr->messages_unread, 1);
    }
    #endif


    if (atomic_load((&current_message_header->has_data)) == 0) {
        printf("ASJDHAKSJDHJKJASD\n");
    }
    
    *buffer = malloc(current_message_header->message_size);
    if (*buffer == NULL) {
        printf("[Reader] Failed to allocate memory for message. has_data: %lld\n", atomic_load(&current_message_header->message_size));
        return SH_ERR_OOM;
    }

    memcpy(*buffer, current_message_header + sizeof(communication_model_message), current_message_header->message_size);
    *buffer_size = current_message_header->message_size;

    atomic_fetch_add(&connection->own_header_shared_ptr->left_space, sizeof(communication_model_message) + current_message_header->message_size);

    atomic_store(&current_message_header->has_data, 0);

    connection->own_current_message_ptr += sizeof(communication_model_message) + current_message_header->message_size;

    return SH_OK;
}