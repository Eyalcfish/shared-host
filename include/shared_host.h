#ifndef SHARED_HOST_H
#define SHARED_HOST_H

#include <stddef.h>

#ifdef _WIN32
    #include <basetsd.h>
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
    SH_ERR_INVALID_PORT = -3, // create: occurs when trying to create a connection with an invalid port name, such as an empty string or a string used by managment threads.

    SH_ERR_MESSAGE_TOO_LONG = -4, // write: occurs when trying to write a message that exceeds the maximum allowed message size.
    SH_ERR_CONNECTION_CLOSED = -5, // write/read: occurs when trying to write to or read from a connection that has been closed. connect: occurs when trying to connect to a closed connection.

    SH_ERR_INVALID_PARAMETER = -6, // occurs when passing invalid parameters to any function, such as a null pointer or an invalid buffer size.

    SH_ERR_UNKNOWN = -100 // an unknown error occurred, this is a catch-all for errors that don't fit into the other categories.
} sh_result_t;

typedef struct shared_host_connection {
    void* ptr;
    size_t size;
    const char* port;
} shared_host_connection;

sh_result_t create_shared_host_connection(const char* port, shared_host_connection** out_connection);

sh_result_t connect_to_shared_host_connection(const char* port, shared_host_connection** out_connection);

sh_result_t write_to_shared_host_connection(shared_host_connection* connection, const char* message);

sh_result_t read_from_shared_host_connection(shared_host_connection* connection, char* buffer, size_t buffer_size);

sh_result_t close_shared_host_connection(shared_host_connection* connection);
 
#ifdef __cplusplus
}
#endif

#endif /* SHARED_HOST_H */