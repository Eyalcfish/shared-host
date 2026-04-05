#include "internal/shm_mapping.h"
#include "internal/communication_model.h"
#include "shared_host.h"


#ifdef _WIN32
    #include <windows.h>
#endif

sh_result_t create_shared_host_connection(const char* port, size_t size, shared_host_connection** out_connection) {
    *out_connection = (shared_host_connection*)malloc(sizeof(shared_host_connection));

    if (*out_connection == NULL) {
        return SH_ERR_OOM;
    }

    sh_result_t result = sh_create_shared_memory(port, size + sizeof(communication_model), *out_connection);

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

    ((communication_model*)((*out_connection)->ptr))->capacity = size;

    return SH_OK;
}

sh_result_t connect_to_shared_host_connection(const char* port, shared_host_connection** out_connection) {
    *out_connection = (shared_host_connection*)malloc(sizeof(shared_host_connection));

    if (*out_connection == NULL) {
        return SH_ERR_OOM;
    }

    sh_result_t result = sh_connect_to_shared_memory(port, *out_connection);

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

    (*out_connection)->size = ((communication_model*)((*out_connection)->ptr))->capacity;
    (*out_connection)->port = port;

    return SH_OK;
}

sh_result_t close_shared_host_connection(shared_host_connection* connection) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    #ifdef _WIN32
        UnmapViewOfFile(connection->ptr);
        CloseHandle(connection->sharedBufferHandle); // maybe unncessery
        CloseHandle(connection->eventHandle);
    #endif

    free(connection);

    return SH_OK;
}

sh_result_t claim_ownership_of_shared_host_connection(shared_host_connection* connection, void** buffer) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model* model = (communication_model*)connection->ptr;

    if (atomic_load(&model->owned) == 1) {
        return SH_ERR_CONNECTION_OWNED;
    }

    atomic_store(&model->owned, 1);

    *buffer = connection->ptr + sizeof(communication_model);

    return SH_OK;
}

sh_result_t lose_ownership_of_shared_host_connection(shared_host_connection* connection) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model* model = (communication_model*)connection->ptr;

    if (atomic_load(&model->owned) == 0) {
        return SH_ERR_CONNECTION_NOT_OWNED;
    }

    atomic_store(&model->owned, 0);
    return SH_OK;
}

sh_result_t send_package_to_shared_host_connection(shared_host_connection* connection, size_t size) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model* model = (communication_model*)connection->ptr;

    if (atomic_load(&model->owned) == 0) {
        return SH_ERR_CONNECTION_NOT_OWNED;
    }

    #ifdef _WIN32
    SetEvent(connection->eventHandle);
    #endif

    atomic_store(&model->message_size, size);
    atomic_store(&model->has_data, 1);
    atomic_store(&model->owned, 0);

    return SH_OK;
}

sh_result_t clear_shared_host_connection(shared_host_connection* connection) {
    communication_model* model = (communication_model*)connection->ptr;

    if (atomic_load(&model->owned) == 1) {
        return SH_ERR_CONNECTION_OWNED; // need to change this
    }

    atomic_store(&model->has_data, 0);

    return SH_OK;
}

sh_result_t write_to_shared_host_connection(shared_host_connection* connection, void* buffer, size_t buffer_size) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model* model = (communication_model*)connection->ptr; 

    if (buffer_size > model->capacity) {
        return SH_ERR_MESSAGE_TOO_LONG;
    }
    if (atomic_load(&model->owned) == 1) {
        return SH_ERR_CONNECTION_OWNED;
    }

    atomic_store(&model->owned, 1);

    memcpy(connection->ptr + sizeof(communication_model), buffer, buffer_size);

    atomic_store(&model->has_data, 1);

    return SH_OK;
}

sh_result_t read_from_shared_host_connection(shared_host_connection* connection, void** buffer, size_t* buffer_size) { // ERROR HANDLING NEEDED
    communication_model* model = (communication_model*)connection->ptr;

    while (atomic_load(&model->owned) == 1) { // very yucky way to deal with people leaving owned connections
        _mm_pause(); 
    }
    
    if (atomic_load(&model->has_data) == 0) {
        #ifdef _WIN32
        WaitForSingleObject(connection->eventHandle, INFINITE);
        #endif
    }

    *buffer = malloc(model->message_size);
    if (*buffer == NULL) {
        return SH_ERR_OOM;
    }
    memcpy(*buffer, connection->ptr + sizeof(communication_model), model->message_size);
    *buffer_size = model->message_size;

    return SH_OK;
}