#include "test_utils.h"

// =============================================================================
// Internal helpers
// =============================================================================

// Get current git commit hash (short)
static void get_git_commit(char *out, size_t out_size) {
    FILE *fp = _popen("git rev-parse --short HEAD 2>NUL", "r");
    if (fp) {
        if (fgets(out, (int)out_size, fp) != NULL) {
            // Strip trailing newline/whitespace
            size_t len = strlen(out);
            while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r' || out[len-1] == ' ')) {
                out[--len] = '\0';
            }
        } else {
            strncpy(out, "unknown", out_size - 1);
        }
        _pclose(fp);
    } else {
        strncpy(out, "unknown", out_size - 1);
    }
}

// Get ISO 8601 timestamp
static void get_timestamp(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(out, out_size, "%Y-%m-%dT%H:%M:%S", t);
}

// Write a single benchmark_results_t as a JSON object to file
static void write_result_json(FILE *fp, benchmark_results_t *r, const char *timestamp, const char *commit) {
    fprintf(fp, "    {\n");
    fprintf(fp, "      \"timestamp\": \"%s\",\n", timestamp);
    fprintf(fp, "      \"commit\": \"%s\",\n", commit);
    fprintf(fp, "      \"mode\": \"%s\",\n", r->mode_name);
    fprintf(fp, "      \"latency\": {\n");
    fprintf(fp, "        \"min_ns\": %.1f,\n", r->latency.min_ns);
    fprintf(fp, "        \"avg_ns\": %.1f,\n", r->latency.avg_ns);
    fprintf(fp, "        \"p50_ns\": %.1f,\n", r->latency.p50_ns);
    fprintf(fp, "        \"p99_ns\": %.1f,\n", r->latency.p99_ns);
    fprintf(fp, "        \"p999_ns\": %.1f,\n", r->latency.p999_ns);
    fprintf(fp, "        \"max_ns\": %.1f\n", r->latency.max_ns);
    fprintf(fp, "      },\n");
    fprintf(fp, "      \"sweep\": [\n");
    for (int i = 0; i < NUM_SWEEP_SIZES; i++) {
        fprintf(fp, "        { \"payload_bytes\": %zu, \"ops_per_sec\": %.0f, \"payload_bw_mbps\": %.2f, \"wire_bw_mbps\": %.2f, \"avg_latency_ns\": %.1f }%s\n",
                SWEEP_SIZES[i], r->ops_per_sec[i], r->payload_bw_mbps[i], r->wire_bw_mbps[i], r->avg_latency_ns[i],
                (i < NUM_SWEEP_SIZES - 1) ? "," : "");
    }
    fprintf(fp, "      ],\n");
    fprintf(fp, "      \"integrity\": { \"seq_corruptions\": %llu, \"data_corruptions\": %llu, \"passed\": %s }\n",
            r->seq_corruptions, r->data_corruptions, r->integrity_passed ? "true" : "false");
    fprintf(fp, "    }");
}

// Read entire file into a malloc'd string (returns NULL on failure)
static char* read_entire_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (len <= 0) {
        fclose(fp);
        return NULL;
    }

    char *buf = (char*) malloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    fread(buf, 1, (size_t)len, fp);
    buf[len] = '\0';
    fclose(fp);
    return buf;
}

// =============================================================================
// JSON EMITTER: save results to benchmarks/history.json
// =============================================================================

void save_results_to_json(benchmark_results_t *fast, benchmark_results_t *fast_zc, benchmark_results_t *slow, benchmark_results_t *slow_zc) {
    _mkdir(HISTORY_DIR);

    char timestamp[64];
    char commit[32];
    get_timestamp(timestamp, sizeof(timestamp));
    get_git_commit(commit, sizeof(commit));

    // Read existing history if it exists
    char *existing = read_entire_file(HISTORY_FILE);

    FILE *fp = fopen(HISTORY_FILE, "w");
    if (!fp) {
        printf("[WARN] Could not open %s for writing.\n", HISTORY_FILE);
        free(existing);
        return;
    }

    if (existing) {
        // Find the last ']' in the existing JSON and insert before it
        char *last_bracket = strrchr(existing, ']');
        if (last_bracket) {
            *last_bracket = '\0'; // Truncate at the ']'
            fprintf(fp, "%s,\n", existing); // Write existing entries + comma
        } else {
            // Malformed, start fresh
            fprintf(fp, "[\n");
        }
        free(existing);
    } else {
        fprintf(fp, "[\n");
    }

    // Write FAST result
    write_result_json(fp, fast, timestamp, commit);
    fprintf(fp, ",\n");

    // Write FAST (ZC) result
    write_result_json(fp, fast_zc, timestamp, commit);
    fprintf(fp, ",\n");

    // Write SLOW result
    write_result_json(fp, slow, timestamp, commit);
    fprintf(fp, ",\n");

    // Write SLOW (ZC) result
    write_result_json(fp, slow_zc, timestamp, commit);
    fprintf(fp, "\n");

    fprintf(fp, "]\n");
    fclose(fp);

    printf("[INFO] Benchmark results saved to %s\n", HISTORY_FILE);
}

// =============================================================================
// JSON PARSER: minimal parser for loading history.json
// =============================================================================

// Skip whitespace
static const char* skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

// Parse a JSON string value (returns pointer past closing quote, writes value to out)
static const char* parse_json_string(const char *p, char *out, size_t out_size) {
    p = skip_ws(p);
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    if (*p == '"') p++;
    return p;
}

// Parse a JSON number value (returns pointer past number, writes value to out)
static const char* parse_json_number(const char *p, double *out) {
    p = skip_ws(p);
    char *end;
    *out = strtod(p, &end);
    return end;
}

// Find a key in the current JSON object scope (returns pointer to the value after ':')
static const char* find_json_key(const char *p, const char *key) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    const char *found = strstr(p, search_key);
    if (!found) return NULL;
    found += strlen(search_key);
    found = skip_ws(found);
    if (*found == ':') found++;
    return skip_ws(found);
}

// Parse a benchmark_results_t from a JSON object string starting at '{'
// Returns 1 on success, 0 on failure
static int parse_result_entry(const char *obj_start, const char *obj_end, benchmark_results_t *r) {
    memset(r, 0, sizeof(benchmark_results_t));

    // Limit search scope to this object
    size_t obj_len = (size_t)(obj_end - obj_start);
    char *obj = (char*) malloc(obj_len + 1);
    memcpy(obj, obj_start, obj_len);
    obj[obj_len] = '\0';

    // Parse mode
    const char *val = find_json_key(obj, "mode");
    if (val) {
        parse_json_string(val, r->mode_name, sizeof(r->mode_name));
        r->mode = (strcmp(r->mode_name, "FAST") == 0) ? SH_FAST_CONNECTION : SH_SLOW_CONNECTION;
    }

    // Parse latency
    val = find_json_key(obj, "latency");
    if (val) {
        const char *lv;
        lv = find_json_key(val, "min_ns");
        if (lv) parse_json_number(lv, &r->latency.min_ns);
        lv = find_json_key(val, "avg_ns");
        if (lv) parse_json_number(lv, &r->latency.avg_ns);
        lv = find_json_key(val, "p50_ns");
        if (lv) parse_json_number(lv, &r->latency.p50_ns);
        lv = find_json_key(val, "p99_ns");
        if (lv) parse_json_number(lv, &r->latency.p99_ns);
        lv = find_json_key(val, "p999_ns");
        if (lv) parse_json_number(lv, &r->latency.p999_ns);
        lv = find_json_key(val, "max_ns");
        if (lv) parse_json_number(lv, &r->latency.max_ns);
    }

    // Parse sweep array
    val = find_json_key(obj, "sweep");
    if (val && *val == '[') {
        const char *p = val + 1;
        for (int i = 0; i < NUM_SWEEP_SIZES; i++) {
            // Find next '{' in sweep array
            const char *entry = strchr(p, '{');
            if (!entry) break;
            const char *lv;
            lv = find_json_key(entry, "ops_per_sec");
            if (lv) parse_json_number(lv, &r->ops_per_sec[i]);
            lv = find_json_key(entry, "payload_bw_mbps");
            if (lv) parse_json_number(lv, &r->payload_bw_mbps[i]);
            lv = find_json_key(entry, "wire_bw_mbps");
            if (lv) parse_json_number(lv, &r->wire_bw_mbps[i]);
            lv = find_json_key(entry, "avg_latency_ns");
            if (lv) parse_json_number(lv, &r->avg_latency_ns[i]);
            // Advance past this entry
            const char *close = strchr(entry, '}');
            if (!close) break;
            p = close + 1;
        }
    }

    // Parse integrity
    val = find_json_key(obj, "integrity");
    if (val) {
        const char *lv;
        double tmp;
        lv = find_json_key(val, "seq_corruptions");
        if (lv) { parse_json_number(lv, &tmp); r->seq_corruptions = (uint64_t)tmp; }
        lv = find_json_key(val, "data_corruptions");
        if (lv) { parse_json_number(lv, &tmp); r->data_corruptions = (uint64_t)tmp; }
        lv = find_json_key(val, "passed");
        if (lv) { r->integrity_passed = (strncmp(lv, "true", 4) == 0) ? 1 : 0; }
    }

    free(obj);
    return 1;
}

// =============================================================================
// REGRESSION COMPARISON
// =============================================================================

// Print regression comparison for one mode
static void print_regression(const char *mode_label, benchmark_results_t *prev, benchmark_results_t *curr, const char *prev_timestamp) {
    printf("============ REGRESSION COMPARISON (%s) vs %s ============\n", mode_label, prev_timestamp);
    printf(" Metric              |      Previous |       Current |   Delta\n");
    printf("----------------------+---------------+---------------+--------\n");

    // Avg Latency
    double lat_delta = (prev->latency.avg_ns > 0) ? ((curr->latency.avg_ns - prev->latency.avg_ns) / prev->latency.avg_ns * 100.0) : 0.0;
    const char *lat_warn = (lat_delta > LATENCY_WARN_PCT || lat_delta < -LATENCY_WARN_PCT) ? " !!!" : "";
    printf(" Avg Latency (ns)    | %11.1f   | %11.1f   | %+.1f%%%s\n",
           prev->latency.avg_ns, curr->latency.avg_ns, lat_delta, lat_warn);

    // P99 Latency
    double p99_delta = (prev->latency.p99_ns > 0) ? ((curr->latency.p99_ns - prev->latency.p99_ns) / prev->latency.p99_ns * 100.0) : 0.0;
    const char *p99_warn = (p99_delta > LATENCY_WARN_PCT || p99_delta < -LATENCY_WARN_PCT) ? " !!!" : "";
    printf(" P99 Latency (ns)    | %11.1f   | %11.1f   | %+.1f%%%s\n",
           prev->latency.p99_ns, curr->latency.p99_ns, p99_delta, p99_warn);

    // Peak throughput (64B)
    double ops_delta = (prev->ops_per_sec[0] > 0) ? ((curr->ops_per_sec[0] - prev->ops_per_sec[0]) / prev->ops_per_sec[0] * 100.0) : 0.0;
    const char *ops_warn = (ops_delta < -THROUGHPUT_WARN_PCT || ops_delta > THROUGHPUT_WARN_PCT) ? " !!!" : "";
    printf(" Peak Ops/s (64B)    | %11.0f   | %11.0f   | %+.1f%%%s\n",
           prev->ops_per_sec[0], curr->ops_per_sec[0], ops_delta, ops_warn);

    // Peak bandwidth (64KB)
    double bw_delta = (prev->payload_bw_mbps[5] > 0) ? ((curr->payload_bw_mbps[5] - prev->payload_bw_mbps[5]) / prev->payload_bw_mbps[5] * 100.0) : 0.0;
    const char *bw_warn = (bw_delta < -THROUGHPUT_WARN_PCT || bw_delta > THROUGHPUT_WARN_PCT) ? " !!!" : "";
    printf(" Peak BW (64KB MB/s) | %11.2f   | %11.2f   | %+.1f%%%s\n",
           prev->payload_bw_mbps[5], curr->payload_bw_mbps[5], bw_delta, bw_warn);

    // Integrity
    printf(" Integrity           | %13s | %13s |     %s\n",
           prev->integrity_passed ? "PASSED" : "FAILED",
           curr->integrity_passed ? "PASSED" : "FAILED",
           (prev->integrity_passed == curr->integrity_passed) ? "OK" : "CHANGED !!!");

    printf("\n");
}

static int find_previous_mode_entry(span_t *entries, int entry_count, const char *target_mode_name, benchmark_results_t *out_prev, char *out_timestamp, size_t ts_size) {
    int start_idx = (entry_count >= 4) ? entry_count - 5 : entry_count - 1;
    for (int i = start_idx; i >= 0; i--) {
        benchmark_results_t tmp;
        if (parse_result_entry(entries[i].start, entries[i].end, &tmp)) {
            if (strcmp(tmp.mode_name, target_mode_name) == 0) {
                *out_prev = tmp;
                if (out_timestamp) {
                    const char *ts = find_json_key(entries[i].start, "timestamp");
                    if (ts) parse_json_string(ts, out_timestamp, ts_size);
                    else strncpy(out_timestamp, "unknown", ts_size - 1);
                }
                return 1;
            }
        }
    }
    return 0;
}

void load_and_compare_history(benchmark_results_t *fast, benchmark_results_t *fast_zc, benchmark_results_t *slow, benchmark_results_t *slow_zc) {
    char *json = read_entire_file(HISTORY_FILE);
    if (!json) {
        printf("[INFO] No previous benchmark history found. Skipping regression comparison.\n");
        return;
    }

    int entry_count = 0;
    const char *p = json;

    span_t *entries = NULL;
    int entries_cap = 0;

    p = strchr(json, '[');
    if (!p) { free(json); return; }
    p++;

    while (*p) {
        p = skip_ws(p);
        if (*p == '{') {
            int depth = 1;
            const char *start = p;
            p++;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
            const char *end = p;

            if (entry_count >= entries_cap) {
                entries_cap = (entries_cap == 0) ? 16 : entries_cap * 2;
                entries = (span_t*) realloc(entries, sizeof(span_t) * entries_cap);
            }
            entries[entry_count].start = start;
            entries[entry_count].end = end;
            entry_count++;
        } else if (*p == ',' || *p == ']') {
            p++;
        } else {
            break;
        }
    }

    printf("\n");
    printf("=================================================================\n");
    printf("            HISTORICAL REGRESSION COMPARISON                      \n");
    printf("=================================================================\n\n");

    benchmark_results_t prev;
    char prev_ts[64] = "unknown";

    if (find_previous_mode_entry(entries, entry_count, fast->mode_name, &prev, prev_ts, sizeof(prev_ts))) {
        print_regression(fast->mode_name, &prev, fast, prev_ts);
    } else {
        printf("[INFO] No previous %s benchmark found for comparison.\n", fast->mode_name);
    }

    if (find_previous_mode_entry(entries, entry_count, fast_zc->mode_name, &prev, prev_ts, sizeof(prev_ts))) {
        print_regression(fast_zc->mode_name, &prev, fast_zc, prev_ts);
    } else {
        printf("[INFO] No previous %s benchmark found for comparison.\n", fast_zc->mode_name);
    }

    if (find_previous_mode_entry(entries, entry_count, slow->mode_name, &prev, prev_ts, sizeof(prev_ts))) {
        print_regression(slow->mode_name, &prev, slow, prev_ts);
    } else {
        printf("[INFO] No previous %s benchmark found for comparison.\n", slow->mode_name);
    }

    if (find_previous_mode_entry(entries, entry_count, slow_zc->mode_name, &prev, prev_ts, sizeof(prev_ts))) {
        print_regression(slow_zc->mode_name, &prev, slow_zc, prev_ts);
    } else {
        printf("[INFO] No previous %s benchmark found for comparison.\n", slow_zc->mode_name);
    }

    free(entries);
    free(json);
}
