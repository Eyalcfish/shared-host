#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_host.h"

#ifdef _WIN32
#include <stdint.h>
#include <windows.h>
#endif

// --- Benchmark Configuration ---
#define WARMUP_MESSAGES 10000
#define MEASURED_MESSAGES 100000
#define TOTAL_MESSAGES (WARMUP_MESSAGES + MEASURED_MESSAGES)

// 64 bytes is standard for TCP_NODELAY (Nagle disabled) latency benchmarks
#define PAYLOAD_SIZE 2 

#pragma pack(push, 1)
typedef struct {
    uint32_t sequence_id;
    char payload[PAYLOAD_SIZE];
} TestMessage;
#pragma pack(pop)

#ifdef _WIN32
// --- The "Server" Thread (TCP Echo Server equivalent) ---
DWORD WINAPI tcp_echo_server_thread(LPVOID param) {
    shared_host_connection* conn = (shared_host_connection*)param;
    
    // Lock this thread to CPU Core 1 to prevent cache misses
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)(1 << 1));
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    void* read_buffer = NULL;
    size_t read_size = 0;
    uint32_t expected_sequence = 0;

    while (expected_sequence < TOTAL_MESSAGES) {
        
        // Block/Spin until data arrives (like recv())
        read_from_shared_host_connection(conn, &read_buffer, &read_size);

        // Immediately echo the exact same data back (like send())
        write_to_shared_host_connection(conn, read_buffer, read_size);

        expected_sequence++;
    }

    return 0;
}
#endif

int main()
{
    printf("Starting Duplex (TCP-Style) Nanosecond Latency Benchmark...\n");

#ifdef _WIN32
    // Lock the main thread (Client) to CPU Core 0
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)(1 << 0));
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    
    LARGE_INTEGER frequency, start_time, end_time;
    QueryPerformanceFrequency(&frequency);
#endif

    // --- 1. Setup Single Duplex Connection ---
    shared_host_connection *server_conn = NULL;
    shared_host_connection *client_conn = NULL;
    assert(create_shared_host_connection("tcp_duplex_port", &client_conn) == SH_OK);

    assert(connect_to_shared_host_connection("tcp_duplex_port", &server_conn) == SH_OK);

#ifdef _WIN32
    // Spin up the Server/Echo thread, passing the server connection
    HANDLE hServerThread = CreateThread(NULL, 0, tcp_echo_server_thread, server_conn, 0, NULL);
    assert(hServerThread != NULL);
#endif

    TestMessage msg;
    memset(msg.payload, 0x01, PAYLOAD_SIZE); 

    double total_latency_ns = 0;
    double min_latency_ns = 1e15; // Start artificially high
    double max_latency_ns = 0;

    void* read_buffer = NULL;
    size_t read_size = 0;

    printf("[Client] Sending %d Warmup Pings, followed by %d Measured Pings...\n", WARMUP_MESSAGES, MEASURED_MESSAGES);

    // --- 2. The Hot Loop (Client Side) ---
    for (uint32_t i = 0; i < TOTAL_MESSAGES; i++) {
        msg.sequence_id = i;

#ifdef _WIN32
        QueryPerformanceCounter(&start_time);
#endif

        // SEND: Push data to the server
        write_to_shared_host_connection(client_conn, &msg.payload, sizeof(msg.payload));

        // RECV: Wait for the server to echo it back
        read_from_shared_host_connection(client_conn, &read_buffer, &read_size);

#ifdef _WIN32
        QueryPerformanceCounter(&end_time);
#endif

        // --- 3. Record Metrics (Skipping Warmup) ---
        if (i >= WARMUP_MESSAGES) {
            // Calculate Round Trip Time (RTT) in nanoseconds
            double rtt_ns = ((double)(end_time.QuadPart - start_time.QuadPart) * 1e9) / (double)frequency.QuadPart;
            
            // One-way latency is half of the RTT
            double one_way_ns = rtt_ns / 2.0;

            total_latency_ns += one_way_ns;
            if (one_way_ns < min_latency_ns) min_latency_ns = one_way_ns;
            if (one_way_ns > max_latency_ns) max_latency_ns = one_way_ns;
        }
    }

#ifdef _WIN32
    // Restore normal thread priority before cleaning up
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    
    // Wait for the server thread to cleanly exit
    WaitForSingleObject(hServerThread, INFINITE);
    CloseHandle(hServerThread);
#endif

    // --- 4. Print Results ---
    double avg_latency_ns = total_latency_ns / MEASURED_MESSAGES;

    printf("\n--- One-Way Latency Results ---\n");
    printf("Min Latency: %.0f ns\n", min_latency_ns);
    printf("Max Latency: %.0f ns\n", max_latency_ns);
    printf("Avg Latency: %.0f ns\n", avg_latency_ns);
    printf("-------------------------------\n");

    // --- 5. Cleanup ---
    close_shared_host_connection(client_conn);
    close_shared_host_connection(server_conn);

    return 0;
}