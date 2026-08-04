#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <synchapi.h>
#include <windows.h>
#include <shared_host.h>
#include <string.h>
#include <stdint.h>
#include <intrin.h> // For _mm_pause()
#include <time.h>
#include <direct.h> // For _mkdir

#define SH_OK               0
#define HEADER_SIZE         16

// Test parameters
#define LATENCY_SAMPLES     1000000   // 1M samples for latency distribution
#define SWEEP_ITERATIONS    500000    // 500k ops per size sweep step
#define STRESS_ITERATIONS   1000000   // 1M variable-size packets for stress test

// Regression thresholds
#define THROUGHPUT_WARN_PCT 15.0      // ±15% throughput variance triggers warning
#define LATENCY_WARN_PCT   25.0      // ±25% latency variance triggers warning

// Sweep payload sizes
#define NUM_SWEEP_SIZES     6
static const size_t SWEEP_SIZES[NUM_SWEEP_SIZES] = { 64, 256, 1024, 4096, 16384, 65536 };

// History file path
#define HISTORY_DIR         "benchmarks"
#define HISTORY_FILE        "benchmarks/history.json"

// =============================================================================
// Structs
// =============================================================================

// Helper struct for latency percentiles
typedef struct {
    double min_ns;
    double max_ns;
    double avg_ns;
    double p50_ns;
    double p99_ns;
    double p999_ns;
} latency_stats_t;

// Complete benchmark results for one mode
typedef struct {
    char mode_name[32];         // "FAST", "FAST (ZC)", etc.
    sh_connection_type mode;
    int is_zero_copy;
    // Phase 1 - Latency
    latency_stats_t latency;
    // Phase 2 - Sweep (per payload size)
    double ops_per_sec[NUM_SWEEP_SIZES];
    double payload_bw_mbps[NUM_SWEEP_SIZES];
    double wire_bw_mbps[NUM_SWEEP_SIZES];
    double avg_latency_ns[NUM_SWEEP_SIZES];
    // Phase 3 - Integrity
    uint64_t seq_corruptions;
    uint64_t data_corruptions;
    int integrity_passed;
} benchmark_results_t;

// =============================================================================
// Inline utility functions
// =============================================================================

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

static inline int compare_doubles(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

static inline const char* mode_to_string(sh_connection_type mode) {
    return (mode == SH_FAST_CONNECTION) ? "FAST" : "SLOW";
}

// =============================================================================
// Function declarations (benchmark.c)
// =============================================================================

void run_benchmark_server(sh_connection_type mode, int use_zero_copy, const char *port_name, benchmark_results_t *results);
void run_benchmark_client(sh_connection_type mode, int use_zero_copy, const char *port_name);
void run_zc_unit_tests(void);

// =============================================================================
// Function declarations (history.c)
// =============================================================================

void save_results_to_json(benchmark_results_t *fast, benchmark_results_t *slow);
void load_and_compare_history(benchmark_results_t *fast, benchmark_results_t *slow);

#endif /* TEST_UTILS_H */
