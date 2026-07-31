#ifndef SHARED_HOST_H
#define SHARED_HOST_H

#include <stddef.h>

#ifdef _WIN32
    #include <basetsd.h>
    #include <windows.h>
    typedef SSIZE_T ssize_t;
#else
    #include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SH_OK = 0, // good good

    SH_ERR_PORT_IN_USE = -1, // create
    SH_ERR_OOM = -2, // create: occurs when trying to create a connection with insufficient memory, read: occurs when trying to copy to a buffer that causes OOM, TODO: maybe also to connect incase i want to allocate seperate lanes.
    SH_ERR_INVALID_PORT = -3, // create: occurs when trying to create a connection with an invalid port name, such as an empty string

    SH_ERR_MESSAGE_TOO_LONG = -4, // write: occurs when trying to write a message that exceeds the maximum allowed message size.
    SH_ERR_CONNECTION_CLOSED = -5, // write/read: occurs when trying to write to or read from a connection that has been closed. connect: occurs when trying to connect to a closed connection.

    SH_ERR_INVALID_PARAMETER = -6, // occurs when passing invalid parameters to any function, such as a null pointer or an invalid buffer size.

    SH_ERR_CONNECTION_OWNED = -7, // occurs when trying to USE a connection that is currently owned by another thread.
    SH_ERR_CONNECTION_NOT_OWNED = -8, // occurs when trying to disown a connection that is not currently owned by the calling thread.

    SH_ERR_UNKNOWN = -100 // an unknown error occurred, this is a catch-all for errors that don't fit into the other categories.
} sh_result_t;

typedef enum {
    SH_FAST_CONNECTION = 0,
    SH_SLOW_CONNECTION = 1,
} sh_connection_type;

typedef struct shared_host_shared_connection_header {
    size_t current_item_offset; // should be a atomic
    size_t last_item_offset; // should be a atomic
} shared_host_shared_connection_header;

typedef struct shared_host_shared_settings_header {
    size_t size;
    sh_connection_type connection_type;
} shared_host_shared_settings_header;

typedef struct shared_host_connection {
    void* own_page_start;
    void* opp_page_start;
    shared_host_shared_connection_header* own_shared_connection_header;
    shared_host_shared_connection_header* opp_shared_connection_header;
    shared_host_shared_settings_header* shared_settings_page_ptr;
    #ifdef _WIN32
    HANDLE shared_settings_page_handle;
    HANDLE own_shared_connection_buffer_handle;
    HANDLE opp_shared_connection_buffer_handle;
    #endif
} shared_host_connection; // TODO: move this implementation to an internal header

//BUFFER A: messages
//BUFFER B: messages

//SETTINGS: (0)settings_header, (settings_header)first_connection_header, (settings_header+connection_header)second_connection_header

sh_result_t create_shared_host_connection(const char* port, shared_host_connection* out_connection);

sh_result_t connect_to_shared_host_connection(const char* port, size_t* size, shared_host_connection* out_connection);

sh_result_t write_to_shared_host_connection(shared_host_connection* connection, void* buffer, size_t buffer_size);

sh_result_t read_from_shared_host_connection(shared_host_connection* connection, void** buffer, size_t* buffer_size);

sh_result_t close_shared_host_connection(shared_host_connection* connection);

char* error_to_string(sh_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* SHARED_HOST_H */
