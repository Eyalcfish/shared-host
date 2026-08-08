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
#include <math.h>    // For sqrt(), fabs()

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
    double std_dev_ns;
    double jitter_ns;      // Average absolute deviation from mean
    double p25_ns;
    double p50_ns;
    double p75_ns;
    double p90_ns;
    double p95_ns;
    double p99_ns;
    double p999_ns;
    double p9999_ns;
} latency_stats_t;

// Per-function timing breakdown
typedef struct {
    double write_ns;       // write_to_shared_host_connection avg time
    double read_ns;        // read_from_shared_host_connection avg time
    double zc_write_ns;    // zc_write_to_shared_host_connection avg time
    double zc_send_ns;     // zc_send_to_shared_host_connection avg time
    double roundtrip_ns;   // Full write+read cycle
} function_timing_t;

// CPU cycle measurements
typedef struct {
    uint64_t write_cycles;
    uint64_t read_cycles;
    uint64_t zc_write_cycles;
    uint64_t zc_send_cycles;
    uint64_t overhead_cycles;  // Connection overhead
} cycle_stats_t;

// Concurrent benchmark results
typedef struct {
    int num_clients;
    double aggregate_throughput;
    double avg_latency_per_client;
    int passed;
} concurrent_results_t;

// Complete benchmark results for one mode
typedef struct {
    char mode_name[32];         // "FAST", "FAST (ZC)", etc.
    sh_connection_type mode;
    int is_zero_copy;
    // Phase 1 - Latency & Jitter
    latency_stats_t latency;
    double* latency_samples;    // Raw samples for analysis
    // Phase 1b - Per-function timing
    function_timing_t func_timing;
    // Phase 1c - CPU cycles (if available)
    cycle_stats_t cycles;
    // Phase 2 - Sweep (per payload size)
    double ops_per_sec[NUM_SWEEP_SIZES];
    double payload_bw_mbps[NUM_SWEEP_SIZES];
    double wire_bw_mbps[NUM_SWEEP_SIZES];
    double avg_latency_ns[NUM_SWEEP_SIZES];
    double std_dev_ns[NUM_SWEEP_SIZES];
    // Phase 3 - Integrity
    uint64_t seq_corruptions;
    uint64_t data_corruptions;
    int integrity_passed;
    // Phase 4 - Concurrent
    concurrent_results_t concurrent[4];  // 1, 2, 4, 8 clients
    // Phase 5 - Memory efficiency
    double memory_efficiency_pct;
} benchmark_results_t;

// Span for JSON parsing (start/end pointers)
typedef struct {
    const char* start;
    const char* end;
} span_t;

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

// RDTSC for CPU cycles (x86 only)
static inline uint64_t rdtsc(void) {
    return __rdtsc();
}

static inline int compare_doubles(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

static inline const char* mode_to_string(sh_connection_type mode) {
    return (mode == SH_FAST_CONNECTION) ? "FAST" : "SLOW";
}

// Calculate standard deviation
static inline double calculate_std_dev(double* samples, int count, double mean) {
    double sum_sq_diff = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = samples[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sqrt(sum_sq_diff / count);
}

// Calculate jitter (average absolute deviation)
static inline double calculate_jitter(double* samples, int count, double mean) {
    double sum_abs_diff = 0.0;
    for (int i = 0; i < count; i++) {
        sum_abs_diff += fabs(samples[i] - mean);
    }
    return sum_abs_diff / count;
}

// Get percentile from sorted array
static inline double get_percentile(double* sorted_samples, int count, double pct) {
    int idx = (int)(count * pct / 100.0);
    if (idx >= count) idx = count - 1;
    if (idx < 0) idx = 0;
    return sorted_samples[idx];
}

// =============================================================================
// Function declarations (benchmark.c)
// =============================================================================

void run_benchmark_server(sh_connection_type mode, int use_zero_copy, const char *port_name, benchmark_results_t *results);
void run_benchmark_client(sh_connection_type mode, int use_zero_copy, const char *port_name);
void run_zc_unit_tests(void);
void run_comprehensive_benchmark(sh_connection_type mode, int use_zero_copy, const char *port_name, benchmark_results_t *results);
void print_comprehensive_results(benchmark_results_t *results);
void run_function_timing_test(sh_connection_type mode, int use_zero_copy, const char *port_name, function_timing_t *timing);
void run_concurrent_test(sh_connection_type mode, int use_zero_copy, const char *port_name, int num_clients, concurrent_results_t *result);

// =============================================================================
// Function declarations (history.c)
// =============================================================================

void save_results_to_json(benchmark_results_t *fast, benchmark_results_t *fast_zc, benchmark_results_t *slow, benchmark_results_t *slow_zc);
void load_and_compare_history(benchmark_results_t *fast, benchmark_results_t *fast_zc, benchmark_results_t *slow, benchmark_results_t *slow_zc);

// =============================================================================
// New test function declarations (in test_main.c or new test file)
// =============================================================================
void run_edge_case_tests(void);
void run_stress_tests(void);
void run_concurrent_client_tests(void);
void run_error_handling_tests(void);
void run_memory_safety_tests(void);

#endif /* TEST_UTILS_H */
