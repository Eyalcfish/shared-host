#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <synchapi.h>
#include <windows.h>
#include <shared_host.h>
#include <string.h>
#include <stdint.h>
#include <intrin.h> // For _mm_pause()

#define SH_OK               0
#define HEADER_SIZE         16

// Test parameters
#define LATENCY_SAMPLES     1000000   // 1M samples for latency distribution
#define SWEEP_ITERATIONS    500000    // 500k ops per size sweep step
#define STRESS_ITERATIONS   1000000   // 1M variable-size packets for stress test

// Helper struct for latency percentiles
typedef struct {
    double min_ns;
    double max_ns;
    double avg_ns;
    double p50_ns;
    double p99_ns;
    double p999_ns;
} latency_stats_t;

// Fast PRNG for data pattern generation (Xorshift32)
static inline uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

// QPC to nanoseconds helper
static inline double ticks_to_ns(LONGLONG ticks, LARGE_INTEGER freq) {
    return ((double)ticks * 1000000000.0) / (double)freq.QuadPart;
}

int compare_doubles(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

// -----------------------------------------------------------------------------
// SERVER THREAD
// -----------------------------------------------------------------------------
void server(char is_soft_locked) {
    shared_host_connection* connection = (shared_host_connection*) malloc(sizeof(shared_host_connection));
    int error = create_shared_host_connection("test", is_soft_locked, connection);
    if (error != SH_OK) {
        printf("[SERVER] Failed to create connection: %s\n", error_to_string(error));
        return;
    }


    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    void* buffer;
    size_t buffer_size;

    printf("=================================================================\n");
    printf("                  SHARED-HOST COMPREHENSIVE SUITE                \n");
    printf("=================================================================\n\n");

    // =========================================================================
    // PHASE 1: LATENCY & JITTER DISTRIBUTION (64 Bytes)
    // =========================================================================
    printf("[PHASE 1] Running Latency & Jitter Distribution (%d samples)...\n", LATENCY_SAMPLES);

    double* latency_samples = (double*) malloc(sizeof(double) * LATENCY_SAMPLES);
    LARGE_INTEGER t_start, t_end;
    double total_ns = 0.0;

    for (int i = 0; i < LATENCY_SAMPLES; i++) {
        QueryPerformanceCounter(&t_start);

        while (read_from_shared_host_connection(connection, &buffer, &buffer_size) != SH_OK) {
            _mm_pause();
        }

        QueryPerformanceCounter(&t_end);

        double elapsed_ns = ticks_to_ns(t_end.QuadPart - t_start.QuadPart, freq);
        latency_samples[i] = elapsed_ns;
        total_ns += elapsed_ns;
    }

    // Compute percentiles
    qsort(latency_samples, LATENCY_SAMPLES, sizeof(double), compare_doubles);

    latency_stats_t stats;
    stats.min_ns  = latency_samples[0];
    stats.max_ns  = latency_samples[LATENCY_SAMPLES - 1];
    stats.avg_ns  = total_ns / LATENCY_SAMPLES;
    stats.p50_ns  = latency_samples[(size_t)(LATENCY_SAMPLES * 0.50)];
    stats.p99_ns  = latency_samples[(size_t)(LATENCY_SAMPLES * 0.99)];
    stats.p999_ns = latency_samples[(size_t)(LATENCY_SAMPLES * 0.999)];
    free(latency_samples);

    printf("  -> Min Latency:    %.1f ns\n", stats.min_ns);
    printf("  -> Avg Latency:    %.1f ns\n", stats.avg_ns);
    printf("  -> P50 (Median):   %.1f ns\n", stats.p50_ns);
    printf("  -> P99 Latency:    %.1f ns\n", stats.p99_ns);
    printf("  -> P99.9 Latency:  %.1f ns\n", stats.p999_ns);
    printf("  -> Max Latency:    %.1f ns\n\n", stats.max_ns);

    // =========================================================================
    // PHASE 2: PAYLOAD SIZE SWEEP
    // =========================================================================
    size_t sweep_sizes[] = { 64, 256, 1024, 4096, 16384, 65536 };
    size_t num_sizes = sizeof(sweep_sizes) / sizeof(sweep_sizes[0]);

    printf("[PHASE 2] Executing Payload Size Sweep (%d ops per size)...\n", SWEEP_ITERATIONS);
    printf(" Payload |    Throughput |   Payload BW |      Wire BW |   Avg Latency\n");
    printf("---------+---------------+--------------+--------------+--------------\n");

    for (size_t s = 0; s < num_sizes; s++) {
        size_t expected_size = sweep_sizes[s];

        QueryPerformanceCounter(&t_start);
        for (int i = 0; i < SWEEP_ITERATIONS; i++) {
            while (read_from_shared_host_connection(connection, &buffer, &buffer_size) != SH_OK) {
                _mm_pause();
            }
        }
        QueryPerformanceCounter(&t_end);

        double elapsed_sec = (double)(t_end.QuadPart - t_start.QuadPart) / (double)freq.QuadPart;
        double ops_sec = SWEEP_ITERATIONS / elapsed_sec;
        double payload_mb = ((double)SWEEP_ITERATIONS * expected_size) / (1024.0 * 1024.0 * elapsed_sec);
        double wire_mb = ((double)SWEEP_ITERATIONS * (expected_size + HEADER_SIZE)) / (1024.0 * 1024.0 * elapsed_sec);
        double avg_lat_ns = (elapsed_sec * 1e9) / SWEEP_ITERATIONS;

        printf(" %6zuB | %9.0f/s | %8.2f MB/s | %8.2f MB/s | %8.1f ns\n",
               expected_size, ops_sec, payload_mb, wire_mb, avg_lat_ns);
    }
    printf("\n");

    // =========================================================================
    // PHASE 3: HIGH-ENTROPY INTEGRITY & RING BUFFER BOUNDARY WRAP (FIXED)
    // =========================================================================
    printf("[PHASE 3] Running Variable-Size Integrity & Boundary Wrap Test (%d ops)...\n", STRESS_ITERATIONS);

    uint64_t seq_corruptions = 0;
    uint64_t data_corruptions = 0;

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        while (read_from_shared_host_connection(connection, &buffer, &buffer_size) != SH_OK) {
            _mm_pause();
        }

        // 1. Verify sequence header in first 8 bytes
        uint64_t seq = *(uint64_t*)buffer;
        if (seq != (uint64_t)i) {
            seq_corruptions++;
        }

        // 2. Verify entire payload byte-for-byte against PRNG seed
        uint32_t seed = (uint32_t)i;
        uint8_t* byte_buf = (uint8_t*)buffer;

        for (size_t b = 8; b < buffer_size; b++) { // Increment by 1 byte
            uint8_t expected_byte = (uint8_t)(xorshift32(&seed) & 0xFF);
            if (byte_buf[b] != expected_byte) {
                data_corruptions++;
                break; // Break payload loop, move to next message
            }
        }
    }

    printf("  -> Sequence Corruptions:     %llu\n", seq_corruptions);
    printf("  -> Byte Content Corruptions: %llu\n", data_corruptions);
    printf("  -> Final Status:             %s\n\n",
            (seq_corruptions == 0 && data_corruptions == 0) ? "PASSED (100% Valid)" : "FAILED");
    printf("=================================================================\n");
}

// -----------------------------------------------------------------------------
// CLIENT THREAD
// -----------------------------------------------------------------------------
void client() {
    shared_host_connection* connection = (shared_host_connection*) malloc(sizeof(shared_host_connection));
    size_t size = 0;

    // Spin until server initializes connection
    while (connect_to_shared_host_connection("test", &size, connection) != SH_OK) {
        Sleep(1);
    }

    // =========================================================================
    // PHASE 1: LATENCY & JITTER DISTRIBUTION
    // =========================================================================
    char payload_64b[64] = {0};
    for (int i = 0; i < LATENCY_SAMPLES; i++) {
        while (write_to_shared_host_connection(connection, payload_64b, 64) != SH_OK) {
            _mm_pause();
        }
    }

    // =========================================================================
    // PHASE 2: PAYLOAD SIZE SWEEP
    // =========================================================================
    size_t sweep_sizes[] = { 64, 256, 1024, 4096, 16384, 65536 };
    size_t num_sizes = sizeof(sweep_sizes) / sizeof(sweep_sizes[0]);

    for (size_t s = 0; s < num_sizes; s++) {
        size_t current_size = sweep_sizes[s];
        char* sweep_buf = (char*) _aligned_malloc(current_size, 64);
        memset(sweep_buf, 0xAB, current_size);

        for (int i = 0; i < SWEEP_ITERATIONS; i++) {
            while (write_to_shared_host_connection(connection, sweep_buf, current_size) != SH_OK) {
                _mm_pause();
            }
        }
        _aligned_free(sweep_buf);
    }

    // =========================================================================
    // PHASE 3: HIGH-ENTROPY INTEGRITY & RING BUFFER BOUNDARY WRAP
    // =========================================================================
    uint32_t size_seed = 0x1337BEEF;
    char dynamic_payload[8192];

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        // Vary size between 32 bytes and 8000 bytes pseudo-randomly
        // Forces non-aligned allocations and forces wrapping across 1KB/4KB ring boundaries
        size_t packet_size = 32 + (xorshift32(&size_seed) % 7968);

        // Embed sequence
        *(uint64_t*)dynamic_payload = (uint64_t)i;

        // Populate pseudo-random bytes
        uint32_t data_seed = (uint32_t)i;
        for (size_t b = 8; b < packet_size; b++) {
            dynamic_payload[b] = (uint8_t)(xorshift32(&data_seed) & 0xFF);
        }

        while (write_to_shared_host_connection(connection, dynamic_payload, packet_size) != SH_OK) {
            _mm_pause();
        }
    }
}
// 1. Create a wrapper function that matches the Win32 ThreadProc signature
DWORD WINAPI server_wrapper(LPVOID lpParam) {
    (void)lpParam;
    server(1);
    return 0; // Thread exit code
}

int main() {
    HANDLE hThread;
    DWORD threadId;

    // 2. Spawn the thread executing the server wrapper
    hThread = CreateThread(
        NULL,                   // Default security attributes
        0,                      // Default stack size (0 = use default)
        server_wrapper,         // Thread function pointer
        NULL,                   // Parameter to pass to the thread
        0,                      // Creation flags (0 = run immediately)
        &threadId               // Pointer to receive the thread identifier
    );

    if (hThread == NULL) {
        fprintf(stderr, "Thread creation failed. Error code: %lu\n", GetLastError());
        return 1;
    }

    Sleep(100);
    // 3. The main thread continues immediately to execute the client
    client();

    // 4. Wait for the server thread to terminate before exiting main
    // INFINITE blocks the main thread indefinitely until the server thread exits.
    WaitForSingleObject(hThread, INFINITE);

    // 5. Close the thread handle to prevent resource leaks
    CloseHandle(hThread);

    return 0;
}
