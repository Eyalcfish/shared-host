#include <shared_host.h>
#include <internal/communication_model.h>
#include "internal/shm_mapping.h"

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
    sh_result_t result = sh_create_shared_memory(port, size+sizeof(communication_model), &handle, &ptr);
    if (result != SH_OK) {
        free(*out_connection);
        *out_connection = NULL;
        return result;
    }

    (*out_connection)->ptr = ptr;
    (*out_connection)->size = size;
    (*out_connection)->port = port;
    (*out_connection)->handle = handle;

    ((communication_model*)ptr)->capacity = size;

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
    (*out_connection)->size = ((communication_model*)ptr)->capacity;
    (*out_connection)->port = port;
    (*out_connection)->handle = handle; // HANDLE IS WINDOWS ONLY

    return SH_OK;
}

sh_result_t close_shared_host_connection(shared_host_connection* connection) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    #ifdef _WIN32
        UnmapViewOfFile(connection->ptr);
        CloseHandle(connection->handle);
    #endif

    free(connection);

    return SH_OK;
}

sh_result_t claim_ownership_of_shared_host_connection(shared_host_connection* connection, void** buffer) {
    if (connection == NULL) {
        return SH_ERR_INVALID_PARAMETER;
    }

    communication_model* model = (communication_model*)connection->ptr;

    if (atomic_load(&model->owned) == 0 && atomic_load(&model->has_data) == 0) {
        atomic_store(&model->owned, 1);
    } else {
        return SH_ERR_CONNECTION_CLOSED; // ai generated this else
    }

    *buffer = connection->ptr+sizeof(communication_model);

    return SH_OK;
}

sh_result_t send_package_to_shared_host_connection(shared_host_connection* connection, void** buffer) {
    communication_model* model = (communication_model*)connection->ptr;
    
    atomic_store(&model->has_data, 1);
    atomic_store(&model->owned, 0);

    return SH_OK;
}

sh_result_t peek_shared_host_connection(shared_host_connection* connection, void** buffer, size_t* buffer_size) {
    communication_model* model = (communication_model*)connection->ptr;

    if (atomic_load(&model->has_data) == 0) {
        return SH_OK; // NEED TO CREATE A SEPERATE ERROR
    }

    size_t peeked = atomic_fetch_add(&model->peeking, 1);

    *buffer = connection->ptr+sizeof(communication_model);
    *buffer_size = model->capacity;

    atomic_store(&model->owned, 1);

    return SH_OK;
}

sh_result_t stop_peeking_shared_host_connection(shared_host_connection* connection) {
    communication_model* model = (communication_model*)connection->ptr;

    atomic_fetch_sub(&model->peeking, 1);

    if (atomic_load(&model->peeking) == 0) {
        atomic_store(&model->owned, 0);
    }

    return SH_OK;
}

sh_result_t clear_shared_host_connection(shared_host_connection* connection) {
    communication_model* model = (communication_model*)connection->ptr;

    if (atomic_load(&model->owned) == 0) {
        atomic_store(&model->has_data, 0);
    } else {
        return SH_ERR_CONNECTION_USED; // ai generated this else
    }
    
    return SH_OK;
}