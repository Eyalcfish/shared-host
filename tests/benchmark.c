#include "test_utils.h"

// =============================================================================
// SERVER: runs all 3 phases, collects results
// =============================================================================
void run_benchmark_server(sh_connection_type mode, benchmark_results_t *results) {
    memset(results, 0, sizeof(benchmark_results_t));
    results->mode = mode;
    strncpy(results->mode_name, mode_to_string(mode), sizeof(results->mode_name) - 1);

    shared_host_connection* connection = (shared_host_connection*) malloc(sizeof(shared_host_connection));
    int error = create_shared_host_connection("test", (char)mode, connection);
    if (error != SH_OK) {
        printf("[SERVER-%s] Failed to create connection: %s\n", results->mode_name, error_to_string(error));
        free(connection);
        return;
    }

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    void* buffer;
    size_t buffer_size;

    printf("=================================================================\n");
    printf("         SHARED-HOST COMPREHENSIVE SUITE [%s MODE]         \n", results->mode_name);
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

    results->latency.min_ns  = latency_samples[0];
    results->latency.max_ns  = latency_samples[LATENCY_SAMPLES - 1];
    results->latency.avg_ns  = total_ns / LATENCY_SAMPLES;
    results->latency.p50_ns  = latency_samples[(size_t)(LATENCY_SAMPLES * 0.50)];
    results->latency.p99_ns  = latency_samples[(size_t)(LATENCY_SAMPLES * 0.99)];
    results->latency.p999_ns = latency_samples[(size_t)(LATENCY_SAMPLES * 0.999)];
    free(latency_samples);

    printf("  -> Min Latency:    %.1f ns\n", results->latency.min_ns);
    printf("  -> Avg Latency:    %.1f ns\n", results->latency.avg_ns);
    printf("  -> P50 (Median):   %.1f ns\n", results->latency.p50_ns);
    printf("  -> P99 Latency:    %.1f ns\n", results->latency.p99_ns);
    printf("  -> P99.9 Latency:  %.1f ns\n", results->latency.p999_ns);
    printf("  -> Max Latency:    %.1f ns\n\n", results->latency.max_ns);

    // =========================================================================
    // PHASE 2: PAYLOAD SIZE SWEEP
    // =========================================================================
    printf("[PHASE 2] Executing Payload Size Sweep (%d ops per size)...\n", SWEEP_ITERATIONS);
    printf(" Payload |    Throughput |   Payload BW |      Wire BW |   Avg Latency\n");
    printf("---------+---------------+--------------+--------------+--------------\n");

    for (size_t s = 0; s < NUM_SWEEP_SIZES; s++) {
        size_t expected_size = SWEEP_SIZES[s];

        QueryPerformanceCounter(&t_start);
        for (int i = 0; i < SWEEP_ITERATIONS; i++) {
            while (read_from_shared_host_connection(connection, &buffer, &buffer_size) != SH_OK) {
                _mm_pause();
            }
        }
        QueryPerformanceCounter(&t_end);

        double elapsed_sec = (double)(t_end.QuadPart - t_start.QuadPart) / (double)freq.QuadPart;
        results->ops_per_sec[s] = SWEEP_ITERATIONS / elapsed_sec;
        results->payload_bw_mbps[s] = ((double)SWEEP_ITERATIONS * expected_size) / (1024.0 * 1024.0 * elapsed_sec);
        results->wire_bw_mbps[s] = ((double)SWEEP_ITERATIONS * (expected_size + HEADER_SIZE)) / (1024.0 * 1024.0 * elapsed_sec);
        results->avg_latency_ns[s] = (elapsed_sec * 1e9) / SWEEP_ITERATIONS;

        printf(" %6zuB | %9.0f/s | %8.2f MB/s | %8.2f MB/s | %8.1f ns\n",
               expected_size, results->ops_per_sec[s], results->payload_bw_mbps[s],
               results->wire_bw_mbps[s], results->avg_latency_ns[s]);
    }
    printf("\n");

    // =========================================================================
    // PHASE 3: HIGH-ENTROPY INTEGRITY & RING BUFFER BOUNDARY WRAP
    // =========================================================================
    printf("[PHASE 3] Running Variable-Size Integrity & Boundary Wrap Test (%d ops)...\n", STRESS_ITERATIONS);

    results->seq_corruptions = 0;
    results->data_corruptions = 0;

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        while (read_from_shared_host_connection(connection, &buffer, &buffer_size) != SH_OK) {
            _mm_pause();
        }

        // 1. Verify sequence header in first 8 bytes
        uint64_t seq = *(uint64_t*)buffer;
        if (seq != (uint64_t)i) {
            results->seq_corruptions++;
        }

        // 2. Verify entire payload byte-for-byte against PRNG seed
        uint32_t seed = (uint32_t)i;
        uint8_t* byte_buf = (uint8_t*)buffer;

        for (size_t b = 8; b < buffer_size; b++) { // Increment by 1 byte
            uint8_t expected_byte = (uint8_t)(xorshift32(&seed) & 0xFF);
            if (byte_buf[b] != expected_byte) {
                results->data_corruptions++;
                break; // Break payload loop, move to next message
            }
        }
    }

    results->integrity_passed = (results->seq_corruptions == 0 && results->data_corruptions == 0);

    printf("  -> Sequence Corruptions:     %llu\n", results->seq_corruptions);
    printf("  -> Byte Content Corruptions: %llu\n", results->data_corruptions);
    printf("  -> Final Status:             %s\n\n",
            results->integrity_passed ? "PASSED (100% Valid)" : "FAILED");
    printf("=================================================================\n\n");

    close_shared_host_connection(connection);
}

// =============================================================================
// CLIENT: sends all 3 phases of data
// =============================================================================
void run_benchmark_client(sh_connection_type mode) {
    (void)mode; // Mode is set by the server; client just connects
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
    for (size_t s = 0; s < NUM_SWEEP_SIZES; s++) {
        size_t current_size = SWEEP_SIZES[s];
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

    close_shared_host_connection(connection);
}
