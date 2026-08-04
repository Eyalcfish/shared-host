#include "test_utils.h"

// =============================================================================
// COMPARISON TABLE: FAST vs SLOW side-by-side
// =============================================================================
static void print_comparison_table(benchmark_results_t *fast, benchmark_results_t *slow) {
    printf("\n");
    printf("=================================================================\n");
    printf("            FAST vs SLOW MODE COMPARISON                         \n");
    printf("=================================================================\n\n");

    printf("--- Phase 1: Latency ---\n");
    printf(" Metric          |       FAST |       SLOW\n");
    printf("------------------+------------+------------\n");
    printf(" Min Latency     | %8.1f ns | %8.1f ns\n", fast->latency.min_ns, slow->latency.min_ns);
    printf(" Avg Latency     | %8.1f ns | %8.1f ns\n", fast->latency.avg_ns, slow->latency.avg_ns);
    printf(" P50 (Median)    | %8.1f ns | %8.1f ns\n", fast->latency.p50_ns, slow->latency.p50_ns);
    printf(" P99 Latency     | %8.1f ns | %8.1f ns\n", fast->latency.p99_ns, slow->latency.p99_ns);
    printf(" P99.9 Latency   | %8.1f ns | %8.1f ns\n", fast->latency.p999_ns, slow->latency.p999_ns);
    printf(" Max Latency     | %8.1f ns | %8.1f ns\n\n", fast->latency.max_ns, slow->latency.max_ns);

    printf("--- Phase 2: Throughput Sweep ---\n");
    printf(" Payload |     FAST ops/s |     SLOW ops/s |    FAST BW |    SLOW BW\n");
    printf("---------+----------------+----------------+------------+------------\n");
    for (int i = 0; i < NUM_SWEEP_SIZES; i++) {
        printf(" %6zuB | %10.0f/s | %10.0f/s | %6.1f MB/s | %6.1f MB/s\n",
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

    printf("--- Phase 1: Latency ---\n");
    printf(" Metric          |     STANDARD |    ZERO-COPY |     Delta\n");
    printf("------------------+--------------+--------------+-----------\n");
    printf(" Avg Latency     | %8.1f ns | %8.1f ns | %+6.1f%%\n",
           std_res->latency.avg_ns, zc_res->latency.avg_ns,
           ((zc_res->latency.avg_ns - std_res->latency.avg_ns) / (std_res->latency.avg_ns > 0 ? std_res->latency.avg_ns : 1.0)) * 100.0);
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
}

// =============================================================================
// ENTRY POINT
// =============================================================================
int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    // ---- 0. Run Zero-Copy Unit Tests ----
    run_zc_unit_tests();

    benchmark_results_t fast_results, slow_results;
    benchmark_results_t fast_zc_results, slow_zc_results;

    printf("\n");
    printf("*****************************************************************\n");
    printf("*     SHARED-HOST DUAL-MODE TEST & BENCHMARK SUITE              *\n");
    printf("*     Testing: STANDARD vs ZERO-COPY (zc_write / zc_send)        *\n");
    printf("*****************************************************************\n\n");

    // ---- Run FAST mode (Standard) ----
    run_mode(SH_FAST_CONNECTION, 0, "test_fast_std", &fast_results);
    Sleep(200);

    // ---- Run FAST mode (Zero-Copy) ----
    run_mode(SH_FAST_CONNECTION, 1, "test_fast_zc", &fast_zc_results);
    Sleep(200);

    // ---- Run SLOW mode (Standard) ----
    run_mode(SH_SLOW_CONNECTION, 0, "test_slow_std", &slow_results);
    Sleep(200);

    // ---- Run SLOW mode (Zero-Copy) ----
    run_mode(SH_SLOW_CONNECTION, 1, "test_slow_zc", &slow_zc_results);

    // ---- Standard FAST vs SLOW comparison ----
    print_comparison_table(&fast_results, &slow_results);

    // ---- Zero-Copy vs Standard comparison ----
    print_zc_comparison_table(&fast_results, &fast_zc_results);
    print_zc_comparison_table(&slow_results, &slow_zc_results);

    // ---- Save to history ----
    save_results_to_json(&fast_results, &slow_results);

    // ---- Historical regression comparison ----
    load_and_compare_history(&fast_results, &slow_results);

    return 0;
}
