#include "test_utils.h"

#include "test_utils.h"
#include <math.h>

// =============================================================================
// COMPARISON TABLE: FAST vs SLOW side-by-side (ENHANCED)
// =============================================================================
static void print_comparison_table(benchmark_results_t *fast, benchmark_results_t *slow) {
    printf("\n");
    printf("=================================================================\n");
    printf("            FAST vs SLOW MODE COMPARISON (ENHANCED)              \n");
    printf("=================================================================\n\n");

    printf("--- Phase 1: Latency & Jitter ---\n");
    printf(" Metric          |       FAST |       SLOW |     Ratio\n");
    printf("------------------+------------+------------+-----------\n");
    printf(" Min Latency     | %8.1f ns | %8.1f ns |   %.2fx\n", fast->latency.min_ns, slow->latency.min_ns, slow->latency.min_ns / (fast->latency.min_ns > 0 ? fast->latency.min_ns : 1));
    printf(" Avg Latency     | %8.1f ns | %8.1f ns |   %.2fx\n", fast->latency.avg_ns, slow->latency.avg_ns, slow->latency.avg_ns / (fast->latency.avg_ns > 0 ? fast->latency.avg_ns : 1));
    printf(" Std Deviation   | %8.1f ns | %8.1f ns |   %.2fx\n", fast->latency.std_dev_ns, slow->latency.std_dev_ns, slow->latency.std_dev_ns / (fast->latency.std_dev_ns > 0 ? fast->latency.std_dev_ns : 1));
    printf(" Jitter          | %8.1f ns | %8.1f ns |   %.2fx\n", fast->latency.jitter_ns, slow->latency.jitter_ns, slow->latency.jitter_ns / (fast->latency.jitter_ns > 0 ? fast->latency.jitter_ns : 1));
    printf(" P50 (Median)    | %8.1f ns | %8.1f ns |   %.2fx\n", fast->latency.p50_ns, slow->latency.p50_ns, slow->latency.p50_ns / (fast->latency.p50_ns > 0 ? fast->latency.p50_ns : 1));
    printf(" P99 Latency     | %8.1f ns | %8.1f ns |   %.2fx\n", fast->latency.p99_ns, slow->latency.p99_ns, slow->latency.p99_ns / (fast->latency.p99_ns > 0 ? fast->latency.p99_ns : 1));
    printf(" P99.9 Latency   | %8.1f ns | %8.1f ns |   %.2fx\n", fast->latency.p999_ns, slow->latency.p999_ns, slow->latency.p999_ns / (fast->latency.p999_ns > 0 ? fast->latency.p999_ns : 1));
    printf(" Max Latency     | %8.1f ns | %8.1f ns |   %.2fx\n\n", fast->latency.max_ns, slow->latency.max_ns, slow->latency.max_ns / (fast->latency.max_ns > 0 ? fast->latency.max_ns : 1));

    printf("--- Phase 2: Throughput Sweep ---\n");
    printf(" Payload |     FAST ops/s |     SLOW ops/s |   FAST BW |   SLOW BW\n");
    printf("---------+----------------+----------------+----------+----------\n");
    for (int i = 0; i < NUM_SWEEP_SIZES; i++) {
        printf(" %6zuB | %10.0f/s | %10.0f/s | %6.1f | %6.1f\n",
               SWEEP_SIZES[i],
               fast->ops_per_sec[i], slow->ops_per_sec[i],
               fast->payload_bw_mbps[i], slow->payload_bw_mbps[i]);
    }

    printf("\n--- Phase 3: Integrity ---\n");
    printf(" FAST: %s  |  SLOW: %s\n\n",
           fast->integrity_passed ? "PASSED" : "FAILED",
           slow->integrity_passed ? "PASSED" : "FAILED");
}

// =============================================================================
// THREAD WRAPPERS & MODE RUNNER
// =============================================================================

// Shared state for thread communication
typedef struct {
    sh_connection_type mode;
    int use_zero_copy;
    char port_name[32];
} thread_params_t;

static thread_params_t g_current_params;
static benchmark_results_t g_current_results;

static DWORD WINAPI server_wrapper(LPVOID lpParam) {
    (void)lpParam;
    run_benchmark_server(g_current_params.mode, g_current_params.use_zero_copy, g_current_params.port_name, &g_current_results);
    return 0;
}

static void run_mode(sh_connection_type mode, int use_zero_copy, const char *port_name, benchmark_results_t *out_results) {
    g_current_params.mode = mode;
    g_current_params.use_zero_copy = use_zero_copy;
    strncpy(g_current_params.port_name, port_name, sizeof(g_current_params.port_name) - 1);

    HANDLE hThread;
    DWORD threadId;

    hThread = CreateThread(
        NULL,
        0,
        server_wrapper,
        NULL,
        0,
        &threadId
    );

    if (hThread == NULL) {
        fprintf(stderr, "Thread creation failed. Error code: %lu\n", GetLastError());
        return;
    }

    Sleep(100);
    run_benchmark_client(mode, use_zero_copy, port_name);

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    *out_results = g_current_results;
}

static void print_zc_comparison_table(benchmark_results_t *std_res, benchmark_results_t *zc_res) {
    printf("\n");
    printf("=================================================================\n");
    printf("     STANDARD (write) vs ZERO-COPY (zc_write/zc_send) [%s]       \n", std_res->mode_name);
    printf("=================================================================\n\n");

    printf("--- Phase 1: Latency & Jitter ---\n");
    printf(" Metric          |     STANDARD |    ZERO-COPY |     Delta\n");
    printf("------------------+--------------+--------------+-----------\n");
    printf(" Avg Latency     | %8.1f ns | %8.1f ns | %+6.1f%%\n",
           std_res->latency.avg_ns, zc_res->latency.avg_ns,
           ((zc_res->latency.avg_ns - std_res->latency.avg_ns) / (std_res->latency.avg_ns > 0 ? std_res->latency.avg_ns : 1.0)) * 100.0);
    printf(" Std Deviation   | %8.1f ns | %8.1f ns | %+6.1f%%\n",
           std_res->latency.std_dev_ns, zc_res->latency.std_dev_ns,
           ((zc_res->latency.std_dev_ns - std_res->latency.std_dev_ns) / (std_res->latency.std_dev_ns > 0 ? std_res->latency.std_dev_ns : 1.0)) * 100.0);
    printf(" Jitter          | %8.1f ns | %8.1f ns | %+6.1f%%\n",
           std_res->latency.jitter_ns, zc_res->latency.jitter_ns,
           ((zc_res->latency.jitter_ns - std_res->latency.jitter_ns) / (std_res->latency.jitter_ns > 0 ? std_res->latency.jitter_ns : 1.0)) * 100.0);
    printf(" P99 Latency     | %8.1f ns | %8.1f ns | %+6.1f%%\n\n",
           std_res->latency.p99_ns, zc_res->latency.p99_ns,
           ((zc_res->latency.p99_ns - std_res->latency.p99_ns) / (std_res->latency.p99_ns > 0 ? std_res->latency.p99_ns : 1.0)) * 100.0);

    printf("--- Phase 2: Throughput Sweep ---\n");
    printf(" Payload |   STANDARD BW |  ZERO-COPY BW | Throughput Gain\n");
    printf("---------+---------------+---------------+----------------\n");
    for (int i = 0; i < NUM_SWEEP_SIZES; i++) {
        double gain = ((zc_res->payload_bw_mbps[i] - std_res->payload_bw_mbps[i]) / (std_res->payload_bw_mbps[i] > 0 ? std_res->payload_bw_mbps[i] : 1.0)) * 100.0;
        printf(" %6zuB | %7.1f MB/s | %7.1f MB/s | %+10.1f%%\n",
               SWEEP_SIZES[i],
               std_res->payload_bw_mbps[i], zc_res->payload_bw_mbps[i],
               gain);
    }

    printf("\n--- Phase 3: Integrity ---\n");
    printf(" STANDARD: %s  |  ZERO-COPY: %s\n\n",
           std_res->integrity_passed ? "PASSED" : "FAILED",
           zc_res->integrity_passed ? "PASSED" : "FAILED");

    printf("--- Per-Function Timing ---\n");
    printf(" Function              |     STANDARD |    ZERO-COPY\n");
    printf("-----------------------+--------------+--------------\n");
    printf(" write/zc_write        | %8.1f ns | %8.1f ns\n", std_res->func_timing.write_ns, zc_res->func_timing.write_ns);
    printf(" read                  | %8.1f ns | %8.1f ns\n", std_res->func_timing.read_ns, zc_res->func_timing.read_ns);
    printf(" zc_send (if ZC)       |         N/A | %8.1f ns\n", zc_res->func_timing.zc_send_ns);
    printf(" Roundtrip             | %8.1f ns | %8.1f ns\n\n", std_res->func_timing.roundtrip_ns, zc_res->func_timing.roundtrip_ns);
}

// =============================================================================
// ENTRY POINT
// =============================================================================
int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("\n");
    printf("*****************************************************************\n");
    printf("*     SHARED-HOST COMPREHENSIVE TEST & BENCHMARK SUITE           *\n");
    printf("*     Version: Enhanced with per-function timing, jitter,       *\n");
    printf("*     concurrent tests, and comprehensive metrics               *\n");
    printf("*****************************************************************\n\n");

    // ---- 0a. Run Zero-Copy Unit Tests ----
    printf("[0a] Running Zero-Copy Unit Tests...\n");
    run_zc_unit_tests();
    Sleep(200);

    // ---- 0b. Run Edge Case Tests ----
    printf("[0b] Running Edge Case Tests...\n");
    run_edge_case_tests();
    Sleep(200);

    // ---- 0c. Run Error Handling & Memory Safety Tests ----
    printf("[0c] Running Error Handling Tests...\n");
    run_error_handling_tests();
    Sleep(200);

    printf("[0d] Running Memory Safety Tests...\n");
    run_memory_safety_tests();
    Sleep(200);

    // ---- 0e. Run Stress & Concurrent Client Tests ----
    printf("[0e] Running Stress Tests...\n");
    run_stress_tests();
    Sleep(200);

    printf("[0f] Running Concurrent Client Tests...\n");
    run_concurrent_client_tests();
    Sleep(200);

    benchmark_results_t fast_results, slow_results;
    benchmark_results_t fast_zc_results, slow_zc_results;

    // ---- Run FAST mode (Standard) ----
    printf("[1] Running FAST mode (Standard)...\n");
    run_mode(SH_FAST_CONNECTION, 0, "test_fast_std", &fast_results);
    run_function_timing_test(SH_FAST_CONNECTION, 0, "test_fast_std_timing", &fast_results.func_timing);
    Sleep(200);

    // ---- Run FAST mode (Zero-Copy) ----
    printf("[2] Running FAST mode (Zero-Copy)...\n");
    run_mode(SH_FAST_CONNECTION, 1, "test_fast_zc", &fast_zc_results);
    run_function_timing_test(SH_FAST_CONNECTION, 1, "test_fast_zc_timing", &fast_zc_results.func_timing);
    Sleep(200);

    // ---- Run SLOW mode (Standard) ----
    printf("[3] Running SLOW mode (Standard)...\n");
    run_mode(SH_SLOW_CONNECTION, 0, "test_slow_std", &slow_results);
    run_function_timing_test(SH_SLOW_CONNECTION, 0, "test_slow_std_timing", &slow_results.func_timing);
    Sleep(200);

    // ---- Run SLOW mode (Zero-Copy) ----
    printf("[4] Running SLOW mode (Zero-Copy)...\n");
    run_mode(SH_SLOW_CONNECTION, 1, "test_slow_zc", &slow_zc_results);
    run_function_timing_test(SH_SLOW_CONNECTION, 1, "test_slow_zc_timing", &slow_zc_results.func_timing);

    // ---- Print all comparison tables ----
    printf("\n");
    printf("*****************************************************************\n");
    printf("*                    BENCHMARK RESULTS SUMMARY                  *\n");
    printf("*****************************************************************\n");

    // Standard FAST vs SLOW comparison
    print_comparison_table(&fast_results, &slow_results);

    // Zero-Copy vs Standard comparison for FAST
    print_zc_comparison_table(&fast_results, &fast_zc_results);
    
    // Zero-Copy vs Standard comparison for SLOW
    print_zc_comparison_table(&slow_results, &slow_zc_results);

    // ---- Save to history ----
    save_results_to_json(&fast_results, &fast_zc_results, &slow_results, &slow_zc_results);

    // ---- Historical regression comparison ----
    load_and_compare_history(&fast_results, &fast_zc_results, &slow_results, &slow_zc_results);

    // ---- Cleanup ----
    if (fast_results.latency_samples) free(fast_results.latency_samples);
    if (slow_results.latency_samples) free(slow_results.latency_samples);
    if (fast_zc_results.latency_samples) free(fast_zc_results.latency_samples);
    if (slow_zc_results.latency_samples) free(slow_zc_results.latency_samples);

    printf("\n");
    printf("*****************************************************************\n");
    printf("*                    TEST SUITE COMPLETE                       *\n");
    printf("*****************************************************************\n");

    return 0;
}
