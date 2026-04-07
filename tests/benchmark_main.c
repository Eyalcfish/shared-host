#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_host.h"

#ifdef _WIN32
#include <stdint.h>
#include <windows.h>
#endif

// --- Batched Benchmark Configuration ---
#define BATCH_SIZE 1000          // Messages per batch
#define WARMUP_BATCHES 10        // Batches to discard
#define MEASURED_BATCHES 1000    // Batches to measure
#define TOTAL_BATCHES (WARMUP_BATCHES + MEASURED_BATCHES)

// 64 bytes is standard for TCP_NODELAY (Nagle disabled) latency benchmarks
#define PAYLOAD_SIZE 64 

#pragma pack(push, 1)
typedef struct {
    uint32_t sequence_id;
    char payload[PAYLOAD_SIZE];
} TestMessage;
#pragma pack(pop)

#ifdef _WIN32
// --- The "Server" Thread ---
DWORD WINAPI tcp_echo_server_thread(LPVOID param) {
    shared_host_connection* conn = (shared_host_connection*)param;
    
    // Lock this thread to CPU Core 1
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)(1 << 1));
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    void* read_buffer = NULL;
    size_t read_size = 0;
    uint32_t expected_sequence = 0;
    uint32_t total_messages = TOTAL_BATCHES * BATCH_SIZE;

    while (expected_sequence < total_messages) {
        
        printf("Server reading\n");
        read_from_shared_host_connection(conn, &read_buffer, &read_size);
        printf("Server read complete\n");
        Sleep(1);

        printf("Server writing\n");
        write_to_shared_host_connection(conn, read_buffer, read_size);
        printf("Server write complete\n");
        Sleep(1);
        expected_sequence++;
    }

    return 0;
}
#endif

int main()
{
    printf("Starting Batched Duplex Latency Benchmark...\n");

#ifdef _WIN32
    // Lock the main thread (Client) to CPU Core 0
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)(1 << 0));
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    
    LARGE_INTEGER frequency, start_time, end_time;
    QueryPerformanceFrequency(&frequency);
#endif

    shared_host_connection *server_conn = NULL;
    assert(create_shared_host_connection("tcp_duplex_port", &server_conn) == SH_OK);

    shared_host_connection *client_conn = NULL;
    assert(connect_to_shared_host_connection("tcp_duplex_port", &client_conn) == SH_OK);

#ifdef _WIN32
    HANDLE hServerThread = CreateThread(NULL, 0, tcp_echo_server_thread, server_conn, 0, NULL);
    assert(hServerThread != NULL);
#endif

    Sleep(1000);

    TestMessage msg;
    memset(msg.payload, 0xAA, PAYLOAD_SIZE); 

    double total_latency_ns = 0;
    double min_latency_ns = 1e15; 
    double max_latency_ns = 0;

    void* read_buffer = NULL;
    size_t read_size = 0;
    uint32_t current_seq = 0;

    printf("[Client] Running %d Warmup Batches...\n", WARMUP_BATCHES);
    printf("[Client] Running %d Measured Batches (%d messages per batch)...\n", MEASURED_BATCHES, BATCH_SIZE);

    // --- The Hot Loop (Batched) ---
    for (uint32_t b = 0; b < TOTAL_BATCHES; b++) {
        
#ifdef _WIN32
        // Start the timer outside the batch
        QueryPerformanceCounter(&start_time);
#endif

        // Pump the entire batch back and forth as fast as possible
        for (uint32_t i = 0; i < BATCH_SIZE; i++) {
            msg.sequence_id = current_seq++;

            printf("[Client] Writing message %u\n", msg.sequence_id);
            write_to_shared_host_connection(client_conn, &msg, sizeof(TestMessage));
            printf("[Client] Write complete for message %u\n", msg.sequence_id);
            Sleep(1);

            printf("[Client] Reading message %u\n", msg.sequence_id);
            read_from_shared_host_connection(client_conn, &read_buffer, &read_size);
            printf("[Client] Read complete for message %u\n", msg.sequence_id);
            Sleep(1);
        }

#ifdef _WIN32
        // Stop the timer after the batch completes
        QueryPerformanceCounter(&end_time);
#endif

        // --- Record Metrics (Skipping Warmup) ---
        if (b >= WARMUP_BATCHES) {
            // Calculate time for the ENTIRE batch
            double batch_time_ns = ((double)(end_time.QuadPart - start_time.QuadPart) * 1e9) / (double)frequency.QuadPart;
            
            // Divide by batch size to get the average Round Trip Time (RTT) per message in this batch
            double rtt_ns = batch_time_ns / (double)BATCH_SIZE;
            
            // One-way latency is half the RTT
            double one_way_ns = rtt_ns / 2.0;

            total_latency_ns += one_way_ns;
            if (one_way_ns < min_latency_ns) min_latency_ns = one_way_ns;
            if (one_way_ns > max_latency_ns) max_latency_ns = one_way_ns;
        }
    }

#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    WaitForSingleObject(hServerThread, INFINITE);
    CloseHandle(hServerThread);
#endif

    double avg_latency_ns = total_latency_ns / MEASURED_BATCHES;

    printf("\n--- Batched One-Way Latency Results ---\n");
    printf("Min Latency: %.0f ns\n", min_latency_ns);
    printf("Max Latency: %.0f ns\n", max_latency_ns);
    printf("Avg Latency: %.0f ns\n", avg_latency_ns);
    printf("---------------------------------------\n");

    close_shared_host_connection(client_conn);
    close_shared_host_connection(server_conn);

    return 0;
}