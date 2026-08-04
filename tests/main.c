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
static sh_connection_type g_current_mode;
static benchmark_results_t g_current_results;

static DWORD WINAPI server_wrapper(LPVOID lpParam) {
    (void)lpParam;
    run_benchmark_server(g_current_mode, &g_current_results);
    return 0;
}

static void run_mode(sh_connection_type mode, benchmark_results_t *out_results) {
    g_current_mode = mode;

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
    run_benchmark_client(mode);

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    *out_results = g_current_results;
}

// =============================================================================
// ENTRY POINT
// =============================================================================
int main() {
    benchmark_results_t fast_results, slow_results;

    printf("\n");
    printf("*****************************************************************\n");
    printf("*     SHARED-HOST DUAL-MODE TEST & BENCHMARK SUITE              *\n");
    printf("*     Testing: SH_FAST_CONNECTION then SH_SLOW_CONNECTION       *\n");
    printf("*****************************************************************\n\n");

    // ---- Run FAST mode ----
    run_mode(SH_FAST_CONNECTION, &fast_results);

    // Small delay between modes to let OS reclaim handles
    Sleep(200);

    // ---- Run SLOW mode ----
    run_mode(SH_SLOW_CONNECTION, &slow_results);

    // ---- Side-by-side comparison ----
    print_comparison_table(&fast_results, &slow_results);

    // ---- Save to history ----
    save_results_to_json(&fast_results, &slow_results);

    // ---- Historical regression comparison ----
    load_and_compare_history(&fast_results, &slow_results);

    return 0;
}
