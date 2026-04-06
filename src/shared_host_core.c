#include "internal/shm_mapping.h"
#include "internal/communication_model.h"
#include "shared_host.h"


#ifdef _WIN32
    #include <windows.h>
#endif

sh_result_t create_shared_host_connection(const char* port, shared_host_connection** out_connection) {
    *out_connection = (shared_host_connection*)malloc(sizeof(shared_host_connection));

    int size = 1000000; // default size, need to make this dynamic later

    if (*out_connection == NULL) {
        return SH_ERR_OOM;
    }

    sh_result_t result = sh_create_shared_memory(port, "server", size + sizeof(communication_model), *out_connection);

    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return result;
    }

    
    result = sh_create_shared_memory(port, "client", size + sizeof(communication_model), *out_connection);

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

    ((communication_model*)((*out_connection)->own_ptr))->capacity = size;
    ((communication_model*)((*out_connection)->opp_ptr))->capacity = size;

    (*out_connection)->own_current_message_ptr = (*out_connection)->own_ptr + sizeof(communication_model);
    (*out_connection)->opp_current_message_ptr = (*out_connection)->opp_ptr + sizeof(communication_model);

    return SH_OK;
}

sh_result_t connect_to_shared_host_connection(const char* port, shared_host_connection** out_connection) {
    *out_connection = (shared_host_connection*)malloc(sizeof(shared_host_connection));

    if (*out_connection == NULL) {
        return SH_ERR_OOM;
    }

    sh_result_t result = sh_connect_to_shared_memory(port, "client", *out_connection);

    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return SH_ERR_CONNECTION_CLOSED;  
    }

    result = sh_connect_to_shared_memory(port, "server", *out_connection);

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

    (*out_connection)->own_current_message_ptr = (*out_connection)->own_ptr + sizeof(communication_model);
    (*out_connection)->opp_current_message_ptr = (*out_connection)->opp_ptr + sizeof(communication_model);
    (*out_connection)->size = ((communication_model*)((*out_connection)->own_ptr))->capacity; // one of those buffers need to be chosen for saving size
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
        CloseHandle(connection->ownSharedBufferHandle);
        CloseHandle(connection->oppSharedBufferHandle);
        CloseHandle(connection->eventHandle);
    #endif

    free(connection);

    return SH_OK;
}

sh_result_t write_to_shared_host_connection(shared_host_connection* connection, void* buffer, size_t buffer_size) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model_message* current_message_header = (communication_model_message*)connection->opp_current_message_ptr;

    if (buffer_size < (size_t) atomic_load(&((communication_model*)connection->opp_ptr)->left_space)) { // NEED TO MAKE THIS SPACE LEFT
        return SH_ERR_MESSAGE_TOO_LONG;
    }

    if (atomic_load(&current_message_header->owned) == 1) {
        return SH_ERR_CONNECTION_OWNED;
    }

    atomic_store(&current_message_header->owned, 1);

    current_message_header->message_size = buffer_size;

    memcpy(current_message_header + sizeof(communication_model_message), buffer, buffer_size);

    atomic_fetch_sub(&((communication_model*)connection->opp_ptr)->left_space, sizeof(communication_model_message) + buffer_size);
    
    atomic_store(&current_message_header->has_data, 1);

    (*connection).opp_current_message_ptr += sizeof(communication_model_message) + buffer_size;
    

    #ifdef _WIN32
    SetEvent(connection->eventHandle);
    #endif

    return SH_OK;
}

sh_result_t read_from_shared_host_connection(shared_host_connection* connection, void** buffer, size_t* buffer_size) { // ERROR HANDLING NEEDED
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model_message* current_message = (communication_model_message*)connection->own_current_message_ptr;

    while (atomic_load(&current_message->owned) == 1) { // very yucky way to deal with people leaving owned connections
        _mm_pause(); 
    }
    
    if (atomic_load(&current_message->has_data) == 0) {
        #ifdef _WIN32
        WaitForSingleObject(connection->eventHandle, INFINITE);
        #endif
    }
    
    atomic_store(&current_message->owned, 1);

    *buffer = malloc(current_message->message_size);
    if (*buffer == NULL) {
        return SH_ERR_OOM;
    }
    memcpy(*buffer, current_message + sizeof(communication_model_message), current_message->message_size);
    *buffer_size = current_message->message_size;

    atomic_fetch_add(&((communication_model*)connection->own_ptr)->left_space, sizeof(communication_model_message) + current_message->message_size);

    atomic_store(&current_message->has_data, 0);
    atomic_store(&current_message->owned, 0);

    return SH_OK;
}