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
    
    printf("Thread woke up. Read size: %zu\n", read_size);
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
    sh_result_t res = create_shared_host_connection("test_port", &conn);
    assert(res == SH_OK);

    shared_host_connection *conn_client = NULL;
    res = connect_to_shared_host_connection("test_port", &conn_client);
    assert(res == SH_OK);

    #ifdef _WIN32
    HANDLE hReaderThread = CreateThread(NULL, 0, test_read_thread, conn_client, 0, NULL);
    assert(hReaderThread != NULL);
    Sleep(2000);
    #endif

    char test_data = 'A';
    res = write_to_shared_host_connection(conn, &test_data, sizeof(test_data));
    assert(res == SH_OK);

    #ifdef _WIN32
    Sleep(500);
    #endif

    res = close_shared_host_connection(conn_client);
    assert(res == SH_OK);

    res = close_shared_host_connection(conn);
    assert(res == SH_OK);

    printf("All tests passed.\n");
    return 0;
}
