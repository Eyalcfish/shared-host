#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_host.h"

#ifdef _WIN32
#include <stdint.h>
#include <windows.h>
#endif

#define PAYLOAD_SIZE 2560

#pragma pack(push, 1)
typedef struct {
    uint32_t sequence_id;
    char payload[PAYLOAD_SIZE];
} TestMessage;
#pragma pack(pop)

#define THROUGHPUT_MESSAGES 5000000

#define BATCH_SIZE 1000          
#define WARMUP_BATCHES 1      
#define MEASURED_BATCHES 100   
#define TOTAL_LATENCY_MESSAGES ((WARMUP_BATCHES + MEASURED_BATCHES) * BATCH_SIZE)

#ifdef _WIN32

DWORD WINAPI throughput_server_thread(LPVOID param) {
    shared_host_connection* conn = (shared_host_connection*)param;
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)(1 << 1));
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    void* read_buffer = NULL;
    size_t read_size = 0;
    uint32_t expected_sequence = 0;

    while (expected_sequence < THROUGHPUT_MESSAGES) {
        if (read_from_shared_host_connection(conn, &read_buffer, &read_size) == SH_OK) {
            expected_sequence++;
            free(read_buffer);
        }
    }
    return 0;
}

DWORD WINAPI latency_server_thread(LPVOID param) {
    shared_host_connection* conn = (shared_host_connection*)param;
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)(1 << 1));
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    void* read_buffer = NULL;
    size_t read_size = 0;
    uint32_t expected_sequence = 0;

    while (expected_sequence < TOTAL_LATENCY_MESSAGES) {
        if (read_from_shared_host_connection(conn, &read_buffer, &read_size) == SH_OK) {
            sh_result_t res;
            do {
                res = write_to_shared_host_connection(conn, read_buffer, read_size);
            } while (res != SH_OK);
            
            expected_sequence++;
            free(read_buffer);
        }
    }
    return 0;
}

int main() {
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)(1 << 0));
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    LARGE_INTEGER frequency, start_time, end_time;
    QueryPerformanceFrequency(&frequency);

    printf("========================================\n");
    printf("  SHARED MEMORY IPC BENCHMARK SUITE\n");
    printf("========================================\n\n");

    printf("[1/2] Running Throughput Test (1-Way Transfer)...\n");
    
    shared_host_connection *tp_server = NULL, *tp_client = NULL;
    assert(create_shared_host_connection("tp_port", &tp_server) == SH_OK);
    assert(connect_to_shared_host_connection("tp_port", &tp_client) == SH_OK);

    HANDLE hTpThread = CreateThread(NULL, 0, throughput_server_thread, tp_server, 0, NULL);
    
    TestMessage msg;
    memset(msg.payload, 0xBB, PAYLOAD_SIZE);

    printf("      Pumping %d messages (%zu bytes each)...\n", THROUGHPUT_MESSAGES, sizeof(TestMessage));
    
    QueryPerformanceCounter(&start_time);
    for (uint32_t i = 0; i < THROUGHPUT_MESSAGES; i++) {
        msg.sequence_id = i;
        sh_result_t res;
        do {
            res = write_to_shared_host_connection(tp_client, &msg, sizeof(TestMessage));
        } while (res != SH_OK);
    }

    WaitForSingleObject(hTpThread, INFINITE);
    QueryPerformanceCounter(&end_time);
    CloseHandle(hTpThread);

    double tp_seconds = (double)(end_time.QuadPart - start_time.QuadPart) / frequency.QuadPart;
    double total_mb = ((double)THROUGHPUT_MESSAGES * sizeof(TestMessage)) / (1024.0 * 1024.0);
    
    printf("      -> Throughput:   %.2f MB/s\n", total_mb / tp_seconds);
    printf("      -> Message Rate: %.0f msgs/sec\n\n", (double)THROUGHPUT_MESSAGES / tp_seconds);

    close_shared_host_connection(tp_client);
    close_shared_host_connection(tp_server);


    printf("[2/2] Running Latency Test (Duplex Ping-Pong)...\n");

    shared_host_connection *lat_server = NULL, *lat_client = NULL;
    assert(create_shared_host_connection("lat_port", &lat_server) == SH_OK);
    assert(connect_to_shared_host_connection("lat_port", &lat_client) == SH_OK);

    HANDLE hLatThread = CreateThread(NULL, 0, latency_server_thread, lat_server, 0, NULL);

    double total_latency_ns = 0, min_latency_ns = 1e15, max_latency_ns = 0;
    void* read_buffer = NULL;
    size_t read_size = 0;
    uint32_t current_seq = 0;

    printf("      Running %d measured batches...\n", MEASURED_BATCHES);

    for (uint32_t b = 0; b < WARMUP_BATCHES + MEASURED_BATCHES; b++) {
        QueryPerformanceCounter(&start_time);

        for (uint32_t i = 0; i < BATCH_SIZE; i++) {
            msg.sequence_id = current_seq++;
            sh_result_t res;

            do {
                res = write_to_shared_host_connection(lat_client, &msg, sizeof(TestMessage));
            } while (res != SH_OK);

            do {
                res = read_from_shared_host_connection(lat_client, &read_buffer, &read_size);
            } while (res != SH_OK);

            free(read_buffer); 
        }

        QueryPerformanceCounter(&end_time);

        if (b >= WARMUP_BATCHES) {
            double batch_ns = ((double)(end_time.QuadPart - start_time.QuadPart) * 1e9) / (double)frequency.QuadPart;
            double one_way_ns = (batch_ns / (double)BATCH_SIZE) / 2.0;

            total_latency_ns += one_way_ns;
            if (one_way_ns < min_latency_ns) min_latency_ns = one_way_ns;
            if (one_way_ns > max_latency_ns) max_latency_ns = one_way_ns;
        }
    }

    WaitForSingleObject(hLatThread, INFINITE);
    CloseHandle(hLatThread);

    printf("      -> Min Latency: %.0f ns\n", min_latency_ns);
    printf("      -> Max Latency: %.0f ns\n", max_latency_ns);
    printf("      -> Avg Latency: %.0f ns\n\n", total_latency_ns / MEASURED_BATCHES);

    close_shared_host_connection(lat_client);
    close_shared_host_connection(lat_server);

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    printf("  BENCHMARK COMPLETE\n");

    return 0;
}
#endif