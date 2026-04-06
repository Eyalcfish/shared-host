#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_host.h"

#ifdef _WIN32
#include <stdint.h>
#include <windows.h>
#endif

// 1. Send enough messages to wrap a 64KB buffer ~1000 times
#define TOTAL_MESSAGES 500000 

// 2. Use a weird, non-power-of-2 size (137 bytes). 
// This forces messages to frequently straddle the physical wrap-around boundary.
#define PAYLOAD_SIZE 137

#pragma pack(push, 1)
typedef struct {
    uint32_t sequence_id;
    char payload[PAYLOAD_SIZE];
} TestMessage;
#pragma pack(pop)

#ifdef _WIN32
DWORD WINAPI test_read_thread(LPVOID param) {
    shared_host_connection* client_conn = (shared_host_connection*)param;
    
    void* read_buffer = NULL;
    size_t read_size = 0;
    uint32_t expected_sequence = 0;

    printf("[Reader] Thread started. Waiting for %d messages...\n", TOTAL_MESSAGES);

    while (expected_sequence < TOTAL_MESSAGES) {
        sh_result_t res = read_from_shared_host_connection(client_conn, &read_buffer, &read_size);
        
        assert(res == SH_OK);
        assert(read_buffer != NULL);
        assert(read_size == sizeof(TestMessage)); 

        TestMessage* msg = (TestMessage*)read_buffer;

        // printf("[Reader] Received message %u/%d\n", expected_sequence + 1, TOTAL_MESSAGES);
        // Data Integrity Check
        assert(msg->sequence_id == expected_sequence);
        
        // Payload Verification (Check first and last byte of the weird payload)
        assert(msg->payload[0] == (char)(expected_sequence % 256));
        assert(msg->payload[PAYLOAD_SIZE - 1] == (char)((expected_sequence + 1) % 256));

        expected_sequence++;

        if (expected_sequence % 100000 == 0) {
            printf("[Reader] Successfully processed %u messages...\n", expected_sequence);
        }
    }

    printf("[Reader] Finished successfully. All sequence IDs matched.\n");
    return 0;
}
#endif

int main()
{
    printf("Starting high-volume circular buffer test...\n");

    shared_host_connection *conn = NULL;
    sh_result_t res = create_shared_host_connection("test_port", &conn);
    assert(res == SH_OK);

    shared_host_connection *conn_client = NULL;
    res = connect_to_shared_host_connection("test_port", &conn_client);
    assert(res == SH_OK);

    #ifdef _WIN32
    HANDLE hReaderThread = CreateThread(NULL, 0, test_read_thread, conn_client, 0, NULL);
    assert(hReaderThread != NULL);
    #endif

    printf("[Writer] Pumping %d messages of size %zu bytes...\n", TOTAL_MESSAGES, sizeof(TestMessage));
    
    TestMessage msg;
    for (uint32_t i = 0; i < TOTAL_MESSAGES; i++) {
        msg.sequence_id = i;
        
        // Fill the payload with some predictable pseudo-random data based on sequence
        memset(msg.payload, (char)(i % 256), PAYLOAD_SIZE);
        msg.payload[PAYLOAD_SIZE - 1] = (char)((i + 1) % 256); // Alter last byte slightly

        res = write_to_shared_host_connection(conn, &msg, sizeof(TestMessage));
        // printf("[Writer] Dispatched message %u/%d\n", i + 1, TOTAL_MESSAGES);
        assert(res == SH_OK);
    }

    printf("[Writer] All messages dispatched. Waiting for reader to finish...\n");

    #ifdef _WIN32
    // Wait infinitely for the reader thread to finish its assertions
    WaitForSingleObject(hReaderThread, INFINITE);
    CloseHandle(hReaderThread);
    #endif

    printf("Closing connections...\n");

    res = close_shared_host_connection(conn_client);
    assert(res == SH_OK);

    res = close_shared_host_connection(conn);
    assert(res == SH_OK);

    printf("MMU Wrap-around Test: PASS.\n");
    return 0;
}