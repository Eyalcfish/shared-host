#include <assert.h>
#include "shared_host.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

DWORD WINAPI test_read_thread(LPVOID param) {
    shared_host_connection* client_conn = (shared_host_connection*)param;
    
    void* read_buffer = NULL;
    size_t read_size = 0;

    printf("Thread started. Waiting for package...\n");

    sh_result_t res = read_from_shared_host_connection(client_conn, &read_buffer, &read_size);
    
    assert(res == SH_OK);
    assert(read_buffer != NULL);
    
    assert(read_size == 1); 

    char received_char = ((char*)read_buffer)[0];
    printf("Woke up Received character: '%c'\n", received_char);
    assert(received_char == 'A');

    return 0;
}
#endif

int main()
{
    printf("Starting unit tests...\n");

    shared_host_connection *conn = NULL;
    sh_result_t res = create_shared_host_connection("test_port", 1024, &conn);
    // if (res != SH_OK) {
    //     switch (res) {
    //         case SH_ERR_PORT_IN_USE:
    //             printf("Error: Port is already in use.\n");
    //             break;
    //         case SH_ERR_OOM:
    //             printf("Error: Out of memory.\n");
    //             break;
    //         case SH_ERR_INVALID_PORT:
    //             printf("Error: Invalid port name.\n");
    //             break;
    //         case SH_ERR_MESSAGE_TOO_LONG:
    //             printf("Error: Message exceeds maximum allowed size.\n");
    //             break;
    //         case SH_ERR_CONNECTION_CLOSED:
    //             printf("Error: Connection is closed.\n");
    //             break;

    //         case SH_ERR_INVALID_PARAMETER:
    //             printf("Error: Invalid parameter provided.\n");
    //             break;

    //         case SH_ERR_CONNECTION_OWNED:
    //             printf("Error: Connection is currently owned by another thread.\n");
    //             break;
    //         case SH_ERR_CONNECTION_NOT_OWNED:
    //             printf("Error: Connection is not owned by the calling thread.\n");
    //             break;

    //         default:
    //             printf("Error: An unknown error occurred.\n");
    //             break;}
    // }
    assert(res == SH_OK);

    shared_host_connection *conn_client = NULL;
    res = connect_to_shared_host_connection("test_port", &conn_client);
    assert(res == SH_OK);

    #ifdef _WIN32
    HANDLE hReaderThread = CreateThread(NULL, 0, test_read_thread, conn_client, 0, NULL);
    assert(hReaderThread != NULL);
    Sleep(2000);
    #endif

    void* buffer = NULL;
    res = claim_ownership_of_shared_host_connection(conn, &buffer);
    assert(res == SH_OK);

    ((char*)buffer)[0] = 'A'; // just testing that the buffer is writable and shared

    res = send_package_to_shared_host_connection(conn, 1); // automatically loses ownership
    assert(res == SH_OK);

    #ifdef _WIN32
    Sleep(50);
    #endif

    res = close_shared_host_connection(conn_client);
    assert(res == SH_OK);

    res = close_shared_host_connection(conn);
    assert(res == SH_OK);

    printf("All tests passed.\n");
    return 0;
}
