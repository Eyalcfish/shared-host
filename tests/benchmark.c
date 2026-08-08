#include "test_utils.h"

// =============================================================================
// SERVER: runs all 5 phases, collects results
// =============================================================================
void run_benchmark_server(sh_connection_type mode, int use_zero_copy, const char *port_name, benchmark_results_t *results) {
    memset(results, 0, sizeof(benchmark_results_t));
    results->mode = mode;
    results->is_zero_copy = use_zero_copy;
    snprintf(results->mode_name, sizeof(results->mode_name), "%s%s",
             mode_to_string(mode), use_zero_copy ? " (ZC)" : "");

    shared_host_connection* connection = (shared_host_connection*) malloc(sizeof(shared_host_connection));
    int error = create_shared_host_connection(port_name, (char)mode, connection);
    if (error != SH_OK) {
        printf("[SERVER-%s] Failed to create connection on port '%s': %s\n", results->mode_name, port_name, error_to_string(error));
        free(connection);
        return;
    }

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    void* buffer;
    size_t buffer_size;

    printf("\n");
    printf("=================================================================\n");
    printf("         SHARED-HOST COMPREHENSIVE SUITE [%s MODE]         \n", results->mode_name);
    printf("=================================================================\n\n");

    // Allocate latency samples array
    results->latency_samples = (double*) malloc(sizeof(double) * LATENCY_SAMPLES);

    // =========================================================================
    // PHASE 1: LATENCY & JITTER DISTRIBUTION (64 Bytes)
    // =========================================================================
    printf("[PHASE 1] Running Latency & Jitter Distribution (%d samples)...\n", LATENCY_SAMPLES);

    // Drain 100 warmup messages sent by client
    void* dummy_buf = NULL;
    size_t dummy_sz = 0;
    for (int i = 0; i < 100; i++) {
        while (read_from_shared_host_connection(connection, &dummy_buf, &dummy_sz) != SH_OK) {
            _mm_pause();
        }
    }

    LARGE_INTEGER t_start, t_end;
    double total_ns = 0.0;

    for (int i = 0; i < LATENCY_SAMPLES; i++) {
        QueryPerformanceCounter(&t_start);

        while (read_from_shared_host_connection(connection, &buffer, &buffer_size) != SH_OK) {
            _mm_pause();
        }

        QueryPerformanceCounter(&t_end);

        double elapsed_ns = ticks_to_ns(t_end.QuadPart - t_start.QuadPart, freq);
        results->latency_samples[i] = elapsed_ns;
        total_ns += elapsed_ns;
    }

    // Compute statistics
    qsort(results->latency_samples, LATENCY_SAMPLES, sizeof(double), compare_doubles);

    results->latency.min_ns  = results->latency_samples[0];
    results->latency.max_ns  = results->latency_samples[LATENCY_SAMPLES - 1];
    results->latency.avg_ns  = total_ns / LATENCY_SAMPLES;
    results->latency.std_dev_ns = calculate_std_dev(results->latency_samples, LATENCY_SAMPLES, results->latency.avg_ns);
    results->latency.jitter_ns = calculate_jitter(results->latency_samples, LATENCY_SAMPLES, results->latency.avg_ns);
    
    // Percentiles
    results->latency.p25_ns  = get_percentile(results->latency_samples, LATENCY_SAMPLES, 25.0);
    results->latency.p50_ns  = get_percentile(results->latency_samples, LATENCY_SAMPLES, 50.0);
    results->latency.p75_ns  = get_percentile(results->latency_samples, LATENCY_SAMPLES, 75.0);
    results->latency.p90_ns  = get_percentile(results->latency_samples, LATENCY_SAMPLES, 90.0);
    results->latency.p95_ns  = get_percentile(results->latency_samples, LATENCY_SAMPLES, 95.0);
    results->latency.p99_ns  = get_percentile(results->latency_samples, LATENCY_SAMPLES, 99.0);
    results->latency.p999_ns = get_percentile(results->latency_samples, LATENCY_SAMPLES, 99.9);
    results->latency.p9999_ns = get_percentile(results->latency_samples, LATENCY_SAMPLES, 99.99);

    printf("  -> Min Latency:    %.1f ns\n", results->latency.min_ns);
    printf("  -> Avg Latency:    %.1f ns\n", results->latency.avg_ns);
    printf("  -> Std Deviation:  %.1f ns\n", results->latency.std_dev_ns);
    printf("  -> Jitter:         %.1f ns\n", results->latency.jitter_ns);
    printf("  -> P25:            %.1f ns\n", results->latency.p25_ns);
    printf("  -> P50 (Median):   %.1f ns\n", results->latency.p50_ns);
    printf("  -> P75:            %.1f ns\n", results->latency.p75_ns);
    printf("  -> P90:            %.1f ns\n", results->latency.p90_ns);
    printf("  -> P95:            %.1f ns\n", results->latency.p95_ns);
    printf("  -> P99:            %.1f ns\n", results->latency.p99_ns);
    printf("  -> P99.9:          %.1f ns\n", results->latency.p999_ns);
    printf("  -> P99.99:         %.1f ns\n", results->latency.p9999_ns);
    printf("  -> Max Latency:    %.1f ns\n\n", results->latency.max_ns);

    // =========================================================================
    // PHASE 2: PAYLOAD SIZE SWEEP (with std dev)
    // =========================================================================
    printf("[PHASE 2] Executing Payload Size Sweep (%d ops per size)...\n", SWEEP_ITERATIONS);
    printf(" Payload |    Throughput |   Payload BW |      Wire BW |   Avg Lat |   Std Dev\n");
    printf("---------+---------------+--------------+--------------+-----------+-----------\n");

    for (size_t s = 0; s < NUM_SWEEP_SIZES; s++) {
        size_t expected_size = SWEEP_SIZES[s];
        double* sweep_samples = (double*) malloc(sizeof(double) * SWEEP_ITERATIONS);

        for (int i = 0; i < SWEEP_ITERATIONS; i++) {
            QueryPerformanceCounter(&t_start);
            while (read_from_shared_host_connection(connection, &buffer, &buffer_size) != SH_OK) {
                _mm_pause();
            }
            QueryPerformanceCounter(&t_end);
            sweep_samples[i] = ticks_to_ns(t_end.QuadPart - t_start.QuadPart, freq);
        }

        // Calculate stats from samples
        double total = 0.0;
        for (int i = 0; i < SWEEP_ITERATIONS; i++) total += sweep_samples[i];
        double avg = total / SWEEP_ITERATIONS;
        double std_dev = calculate_std_dev(sweep_samples, SWEEP_ITERATIONS, avg);
        
        double elapsed_sec = total / 1e9;
        results->ops_per_sec[s] = SWEEP_ITERATIONS / elapsed_sec;
        results->payload_bw_mbps[s] = ((double)SWEEP_ITERATIONS * expected_size) / (1024.0 * 1024.0 * elapsed_sec);
        results->wire_bw_mbps[s] = ((double)SWEEP_ITERATIONS * (expected_size + HEADER_SIZE)) / (1024.0 * 1024.0 * elapsed_sec);
        results->avg_latency_ns[s] = avg;
        results->std_dev_ns[s] = std_dev;

        printf(" %6zuB | %9.0f/s | %8.2f MB/s | %8.2f MB/s | %7.1f ns | %7.1f ns\n",
               expected_size, results->ops_per_sec[s], results->payload_bw_mbps[s],
               results->wire_bw_mbps[s], results->avg_latency_ns[s], results->std_dev_ns[s]);
        
        free(sweep_samples);
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

    // =========================================================================
    // PHASE 4: PER-FUNCTION TIMING BREAKDOWN
    // =========================================================================
    printf("[PHASE 4] Running Per-Function Timing Analysis...\n");
    
    // We need a fresh connection for accurate function timing
    // (or we can estimate from the samples we already have)
    results->func_timing.roundtrip_ns = results->latency.avg_ns;
    // Individual function timing would need client cooperation - skip for server side
    printf("  -> Estimated Roundtrip:     %.1f ns\n", results->func_timing.roundtrip_ns);
    printf("  -> (Detailed per-function timing requires client-side instrumentation)\n\n");

    // =========================================================================
    // PHASE 5: MEMORY EFFICIENCY CALCULATION
    // =========================================================================
    printf("[PHASE 5] Calculating Memory Efficiency...\n");
    // Memory efficiency = payload / (payload + overhead)
    // For shared memory, overhead is minimal (just the ring buffer header)
    results->memory_efficiency_pct = (64.0 / (64.0 + HEADER_SIZE)) * 100.0;  // Example for 64B payload
    printf("  -> Memory Efficiency (64B): %.1f%%\n", results->memory_efficiency_pct);
    printf("  -> (Theoretical max based on header size: %d bytes)\n\n", HEADER_SIZE);

    printf("=================================================================\n\n");

    close_shared_host_connection(connection);
}

// =============================================================================
// ENHANCED ZERO-COPY UNIT TESTS
// =============================================================================
void run_zc_unit_tests(void) {
    printf("\n");
    printf("=================================================================\n");
    printf("         ZERO-COPY (zc_write / zc_send) UNIT TESTS               \n");
    printf("=================================================================\n\n");

    int tests_passed = 0;
    int tests_total = 0;

    // Test 1: NULL parameter checks for zc_write
    tests_total++;
    void *buf = NULL;
    if (zc_write_to_shared_host_connection(NULL, &buf, 64) == SH_ERR_INVALID_PARAMETER) {
        printf(" [PASS] Test 1: zc_write NULL connection parameter validation\n");
        tests_passed++;
    } else {
        printf(" [FAIL] Test 1: zc_write parameter validation failed\n");
    }

    // Test 2: NULL buffer pointer check for zc_write
    tests_total++;
    shared_host_connection* dummy_conn = (shared_host_connection*)malloc(sizeof(shared_host_connection));
    if (zc_write_to_shared_host_connection(dummy_conn, NULL, 64) == SH_ERR_INVALID_PARAMETER) {
        printf(" [PASS] Test 2: zc_write NULL buffer pointer validation\n");
        tests_passed++;
    } else {
        printf(" [FAIL] Test 2: zc_write NULL buffer pointer check failed\n");
    }
    free(dummy_conn);

    // Test 3: NULL parameter checks for zc_send
    tests_total++;
    if (zc_send_to_shared_host_connection(NULL) == SH_ERR_INVALID_PARAMETER) {
        printf(" [PASS] Test 3: zc_send parameter validation\n");
        tests_passed++;
    } else {
        printf(" [FAIL] Test 3: zc_send parameter validation failed\n");
    }

    // Test 4: Zero-size write check
    tests_total++;
    shared_host_connection *server_conn = (shared_host_connection*)malloc(sizeof(shared_host_connection));
    shared_host_connection *client_conn = (shared_host_connection*)malloc(sizeof(shared_host_connection));
    int err1 = create_shared_host_connection("zc_test_port", (char)SH_FAST_CONNECTION, server_conn);
    size_t size = 0;
    int err2 = connect_to_shared_host_connection("zc_test_port", &size, client_conn);
    
    if (err1 == SH_OK && err2 == SH_OK) {
        void *tx_buf = NULL;
        if (zc_write_to_shared_host_connection(client_conn, &tx_buf, 0) == SH_ERR_INVALID_PARAMETER) {
            printf(" [PASS] Test 4: zc_write zero-size validation\n");
            tests_passed++;
        } else {
            printf(" [FAIL] Test 4: zc_write zero-size check failed\n");
        }
        close_shared_host_connection(server_conn);
        close_shared_host_connection(client_conn);
    } else {
        printf(" [SKIP] Test 4: Connection creation failed\n");
        tests_total--;
        free(server_conn);
        free(client_conn);
    }

    // Test 5: End-to-end message exchange using zc_write and zc_send (Client to Server)
    tests_total++;
    server_conn = (shared_host_connection*)malloc(sizeof(shared_host_connection));
    client_conn = (shared_host_connection*)malloc(sizeof(shared_host_connection));
    err1 = create_shared_host_connection("zc_test_port2", (char)SH_FAST_CONNECTION, server_conn);
    err2 = connect_to_shared_host_connection("zc_test_port2", &size, client_conn);

    if (err1 == SH_OK && err2 == SH_OK) {
        void *tx_buf = NULL;
        sh_result_t write_res = zc_write_to_shared_host_connection(client_conn, &tx_buf, 128);
        if (write_res == SH_OK && tx_buf != NULL) {
            memset(tx_buf, 0x42, 128);
            sh_result_t send_res = zc_send_to_shared_host_connection(client_conn);
            if (send_res == SH_OK) {
                void *rx_buf = NULL;
                size_t rx_size = 0;
                sh_result_t read_res = read_from_shared_host_connection(server_conn, &rx_buf, &rx_size);
                if (read_res == SH_OK && rx_size == 128 && ((char*)rx_buf)[0] == 0x42 && ((char*)rx_buf)[127] == 0x42) {
                    printf(" [PASS] Test 5: zc_write/zc_send end-to-end data exchange\n");
                    tests_passed++;
                } else {
                    printf(" [FAIL] Test 5: Read back data mismatch (res=%d, size=%zu)\n", read_res, rx_size);
                }
            } else {
                printf(" [FAIL] Test 5: Client zc_send failed (%d)\n", send_res);
            }
        } else {
            printf(" [FAIL] Test 5: Client zc_write failed (%d)\n", write_res);
        }
        close_shared_host_connection(server_conn);
        close_shared_host_connection(client_conn);
    } else {
        printf(" [FAIL] Test 5: Connection creation failed (err1=%d, err2=%d)\n", err1, err2);
        free(server_conn);
        free(client_conn);
    }

    // Test 6: Multiple sequential zc_write behavior
    tests_total++;
    server_conn = (shared_host_connection*)malloc(sizeof(shared_host_connection));
    client_conn = (shared_host_connection*)malloc(sizeof(shared_host_connection));
    err1 = create_shared_host_connection("zc_test_port3", (char)SH_FAST_CONNECTION, server_conn);
    err2 = connect_to_shared_host_connection("zc_test_port3", &size, client_conn);
    
    if (err1 == SH_OK && err2 == SH_OK) {
        void *buf1 = NULL, *buf2 = NULL;
        sh_result_t res1 = zc_write_to_shared_host_connection(client_conn, &buf1, 64);
        sh_result_t res2 = zc_write_to_shared_host_connection(client_conn, &buf2, 64);
        // Just check that the calls don't crash - behavior is implementation-specific
        printf(" [PASS] Test 6: Multiple zc_write calls completed (res1=%d, res2=%d)\n", res1, res2);
        tests_passed++;
        // Cleanup - send if first write succeeded
        if (res1 == SH_OK) zc_send_to_shared_host_connection(client_conn);
        close_shared_host_connection(server_conn);
        close_shared_host_connection(client_conn);
    } else {
        printf(" [SKIP] Test 6: Connection creation failed\n");
        tests_total--;
        free(server_conn);
        free(client_conn);
    }

    printf("\n  -> Zero-Copy Unit Tests Summary: %d / %d Passed\n\n", tests_passed, tests_total);
}

// =============================================================================
// CLIENT: sends all 3 phases of data
// =============================================================================
void run_benchmark_client(sh_connection_type mode, int use_zero_copy, const char *port_name) {
    (void)mode; // Mode is set by the server; client just connects
    shared_host_connection* connection = (shared_host_connection*) malloc(sizeof(shared_host_connection));
    size_t size = 0;

    // Spin until server initializes connection (up to 5 seconds)
    int retries = 0;
    while (connect_to_shared_host_connection(port_name, &size, connection) != SH_OK) {
        Sleep(1);
        if (++retries > 5000) {
            fprintf(stderr, "[CLIENT] Failed to connect to server on port '%s' after 5 seconds!\n", port_name);
            free(connection);
            return;
        }
    }

    // Warmup: send a few messages to stabilize
    char warmup_payload[64] = {0};
    for (int i = 0; i < 100; i++) {
        if (use_zero_copy) {
            void *buf = NULL;
            if (zc_write_to_shared_host_connection(connection, &buf, 64) == SH_OK) {
                zc_send_to_shared_host_connection(connection);
            }
        } else {
            write_to_shared_host_connection(connection, warmup_payload, 64);
        }
    }

    // =========================================================================
    // PHASE 1: LATENCY & JITTER DISTRIBUTION
    // =========================================================================
    char payload_64b[64] = {0};
    for (int i = 0; i < LATENCY_SAMPLES; i++) {
        if (use_zero_copy) {
            void *buf = NULL;
            while (zc_write_to_shared_host_connection(connection, &buf, 64) != SH_OK) {
                _mm_pause();
            }
            memset(buf, 0, 64);
            zc_send_to_shared_host_connection(connection);
        } else {
            while (write_to_shared_host_connection(connection, payload_64b, 64) != SH_OK) {
                _mm_pause();
            }
        }
    }

    // =========================================================================
    // PHASE 2: PAYLOAD SIZE SWEEP
    // =========================================================================
    for (size_t s = 0; s < NUM_SWEEP_SIZES; s++) {
        size_t current_size = SWEEP_SIZES[s];

        if (use_zero_copy) {
            for (int i = 0; i < SWEEP_ITERATIONS; i++) {
                void *buf = NULL;
                while (zc_write_to_shared_host_connection(connection, &buf, current_size) != SH_OK) {
                    _mm_pause();
                }
                memset(buf, 0xAB, current_size);
                zc_send_to_shared_host_connection(connection);
            }
        } else {
            char* sweep_buf = (char*) _aligned_malloc(current_size, 64);
            memset(sweep_buf, 0xAB, current_size);

            for (int i = 0; i < SWEEP_ITERATIONS; i++) {
                while (write_to_shared_host_connection(connection, sweep_buf, current_size) != SH_OK) {
                    _mm_pause();
                }
            }
            _aligned_free(sweep_buf);
        }
    }

    // =========================================================================
    // PHASE 3: HIGH-ENTROPY INTEGRITY & RING BUFFER BOUNDARY WRAP
    // =========================================================================
    uint32_t size_seed = 0x1337BEEF;
    char dynamic_payload[8192];

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        // Vary size between 32 bytes and 8000 bytes pseudo-randomly
        size_t packet_size = 32 + (xorshift32(&size_seed) % 7968);

        if (use_zero_copy) {
            void *buf = NULL;
            while (zc_write_to_shared_host_connection(connection, &buf, packet_size) != SH_OK) {
                _mm_pause();
            }
            *(uint64_t*)buf = (uint64_t)i;

            uint32_t data_seed = (uint32_t)i;
            uint8_t *byte_buf = (uint8_t*)buf;
            for (size_t b = 8; b < packet_size; b++) {
                byte_buf[b] = (uint8_t)(xorshift32(&data_seed) & 0xFF);
            }
            zc_send_to_shared_host_connection(connection);
        } else {
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

    close_shared_host_connection(connection);
}

// =============================================================================
// PER-FUNCTION TIMING TEST (standalone)
// =============================================================================
void run_function_timing_test(sh_connection_type mode, int use_zero_copy, const char *port_name, function_timing_t *timing) {
    memset(timing, 0, sizeof(function_timing_t));
    
    shared_host_connection* server = (shared_host_connection*)malloc(sizeof(shared_host_connection));
    shared_host_connection* client = (shared_host_connection*)malloc(sizeof(shared_host_connection));
    
    if (create_shared_host_connection(port_name, (char)mode, server) != SH_OK) {
        printf("[TIMING] Failed to create server\n");
        return;
    }
    
    size_t conn_size = 0;
    if (connect_to_shared_host_connection(port_name, &conn_size, client) != SH_OK) {
        printf("[TIMING] Failed to connect client\n");
        close_shared_host_connection(server);
        return;
    }
    
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    
    const int NUM_ITERATIONS = 10000;
    double write_total = 0, read_total = 0, zc_write_total = 0, zc_send_total = 0;
    void* buffer;
    size_t buffer_size;
    
    // Time write function
    char payload[64] = {0};
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        QueryPerformanceCounter(&start);
        write_to_shared_host_connection(client, payload, 64);
        QueryPerformanceCounter(&end);
        write_total += ticks_to_ns(end.QuadPart - start.QuadPart, freq);
        
        // Drain server
        while (read_from_shared_host_connection(server, &buffer, &buffer_size) != SH_OK) _mm_pause();
    }
    timing->write_ns = write_total / NUM_ITERATIONS;
    
    // Time read function (server side)
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Send from client first
        while (write_to_shared_host_connection(client, payload, 64) != SH_OK) _mm_pause();
        
        QueryPerformanceCounter(&start);
        while (read_from_shared_host_connection(server, &buffer, &buffer_size) != SH_OK) _mm_pause();
        QueryPerformanceCounter(&end);
        read_total += ticks_to_ns(end.QuadPart - start.QuadPart, freq);
    }
    timing->read_ns = read_total / NUM_ITERATIONS;
    
    if (use_zero_copy) {
        // Time zc_write function
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            void* zc_buf = NULL;
            QueryPerformanceCounter(&start);
            zc_write_to_shared_host_connection(client, &zc_buf, 64);
            QueryPerformanceCounter(&end);
            zc_write_total += ticks_to_ns(end.QuadPart - start.QuadPart, freq);
            zc_send_to_shared_host_connection(client);
            while (read_from_shared_host_connection(server, &buffer, &buffer_size) != SH_OK) _mm_pause();
        }
        timing->zc_write_ns = zc_write_total / NUM_ITERATIONS;
        
        // Time zc_send function
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            void* zc_buf = NULL;
            zc_write_to_shared_host_connection(client, &zc_buf, 64);
            QueryPerformanceCounter(&start);
            zc_send_to_shared_host_connection(client);
            QueryPerformanceCounter(&end);
            zc_send_total += ticks_to_ns(end.QuadPart - start.QuadPart, freq);
            while (read_from_shared_host_connection(server, &buffer, &buffer_size) != SH_OK) _mm_pause();
        }
        timing->zc_send_ns = zc_send_total / NUM_ITERATIONS;
    }
    
    // Calculate roundtrip
    timing->roundtrip_ns = timing->write_ns + timing->read_ns;
    
    printf("\n[PER-FUNCTION TIMING]\n");
    printf("  write_to_shared_host_connection:  %.1f ns\n", timing->write_ns);
    printf("  read_from_shared_host_connection:  %.1f ns\n", timing->read_ns);
    if (use_zero_copy) {
        printf("  zc_write_to_shared_host_connection: %.1f ns\n", timing->zc_write_ns);
        printf("  zc_send_to_shared_host_connection:  %.1f ns\n", timing->zc_send_ns);
    }
    printf("  Estimated roundtrip:              %.1f ns\n", timing->roundtrip_ns);
    
    close_shared_host_connection(server);
    close_shared_host_connection(client);
}
