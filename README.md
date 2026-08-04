# shared-host

A high-performance, ultra-low-latency Inter-Process Communication (IPC) library written in C. `shared-host` leverages shared memory ring buffers for fast, reliable message passing between processes.

Basically a faster localhost-like communication method.

---

## Ai Usage Guidelines

- Ai in this project was only used for documentation testing and external tools.
- Everything in the `src` and `include` folders was written by me without ai assistance.

---

## Performance Statistics

```text
=================================================================
                  SHARED-HOST COMPREHENSIVE SUITE
=================================================================

[PHASE 1] Running Latency & Jitter Distribution (1000000 samples)...
  -> Min Latency:    0.0 ns
  -> Avg Latency:    188.7 ns
  -> P50 (Median):   0.0 ns
  -> P99 Latency:    1000.0 ns
  -> P99.9 Latency:  2300.0 ns
  -> Max Latency:    114656300.0 ns

[PHASE 2] Executing Payload Size Sweep (500000 ops per size)...
 Payload |    Throughput |   Payload BW |      Wire BW |   Avg Latency
---------+---------------+--------------+--------------+--------------
     64B |  14607198/s |   891.55 MB/s |  1114.44 MB/s |     68.5 ns
    256B |   5874378/s |  1434.17 MB/s |  1523.81 MB/s |    170.2 ns
   1024B |   2420505/s |  2363.77 MB/s |  2400.71 MB/s |    413.1 ns
   4096B |    658398/s |  2571.87 MB/s |  2581.92 MB/s |   1518.8 ns
  16384B |    453552/s |  7086.75 MB/s |  7093.67 MB/s |   2204.8 ns
  65536B |    186576/s | 11661.01 MB/s | 11663.86 MB/s |   5359.7 ns

[PHASE 3] Running Variable-Size Integrity & Boundary Wrap Test (1000000 ops)...
  -> Sequence Corruptions:     0
  -> Byte Content Corruptions: 0
  -> Final Status:             PASSED (100% Valid)

=================================================================
```

### Performance Summary

| Metric / Payload | Result |
| :--- | :--- |
| **Average Latency (Phase 1)** | 188.7 ns (P50: 0.0 ns, P99: 1000.0 ns) |
| **Peak Throughput (64B)** | **14,607,198 ops/sec** (68.5 ns avg latency) |
| **Peak Bandwidth (64KB)** | **11,661.01 MB/s** Payload / **11,663.86 MB/s** Wire |
| **Data Integrity (Phase 3)** | **100% Valid** (0 sequence/byte corruptions over 1M ops) |

---

## Features

- **Ultra-Low Latency**: Sub-microsecond message delivery (~188.7 ns avg latency).
- **High Throughput**: Exceeds 14 Million ops/sec on 64B payloads and 11.6 GB/s bandwidth on 64KB payloads.
- **Flexible Connection Modes**: Supports `SH_FAST_CONNECTION` (spin-polling for ultra-low latency) and `SH_SLOW_CONNECTION` (OS event synchronization for low CPU utilization).
- **Zero Corruption Guarantee**: Includes boundary wrap validation and sequence tracking.
- **Clean C API**: Host connection creation, connection attachment, non-blocking read/write operations, and resource cleanup.
- **Cross-Platform Makefile**: Supports building shared libraries (`.dll` / `.so`) and test/benchmark suites.

---

## Building & Usage

### Using the Makefile

| Target | Description |
| :--- | :--- |
| `make all` | Builds DLL (`build/shared-host.dll`) and test/benchmark binaries |
| `make dll` | Compiles shared library (`build/shared-host.dll`) |
| `make test` | Builds test binary (`build/test_main.exe`) |
| `make benchmark` | Builds benchmark binary (`build/benchmark_main.exe`) |
| `make run-test` | Builds and runs the test suite |
| `make run-benchmark` | Builds and runs the benchmark suite |
| `make clean` | Removes `obj/` and `build/` directories |
| `make help` | Displays list of Makefile targets |

### Using Helper Scripts

Batch scripts (Windows) and shell scripts (Linux/macOS) are located in `scripts/`:

- **Windows Batch**: `scripts\all.bat`, `scripts\make.bat`, `scripts\test.bat`, `scripts\benchmark.bat`
- **Shell**: `./scripts/all.sh`, `./scripts/make.sh`, `./scripts/test.sh`, `./scripts/benchmark.sh`

---

## API Overview

Header file: `#include <shared_host.h>`

### Connection Modes (`sh_connection_type`)

When creating a host connection via `create_shared_host_connection`, pass a mode flag to control synchronization behavior:

| Flag / Enum | Value | Description |
| :--- | :--- | :--- |
| `SH_FAST_CONNECTION` | `0` | **Spin-polling / Yield mode**: Busy-spins using `YieldProcessor()` / `pause` for sub-microsecond latency and maximum throughput. Best for high-frequency, real-time IPC. |
| `SH_SLOW_CONNECTION` | `1` | **Event-driven mode**: Uses OS event handle synchronization (`WaitForSingleObject`) to sleep until data arrives. Minimizes CPU usage when waiting for messages. |

### Code Example

```c
#include <stdio.h>
#include <stdlib.h>
#include <shared_host.h>

// 1. Create a server host connection (passing SH_FAST_CONNECTION or SH_SLOW_CONNECTION)
shared_host_connection server_conn;
sh_result_t err = create_shared_host_connection("my_channel", SH_FAST_CONNECTION, &server_conn);

// 2. Connect client to host
shared_host_connection client_conn;
size_t conn_size = 0;
err = connect_to_shared_host_connection("my_channel", &conn_size, &client_conn);

// 3. Write data from client
char data[] = "High speed IPC payload";
write_to_shared_host_connection(&client_conn, data, sizeof(data));

// 4. Read data on server
void* read_buffer = NULL;
size_t read_bytes = 0;
read_from_shared_host_connection(&server_conn, &read_buffer, &read_bytes);

// 5. Close connection
close_shared_host_connection(&server_conn);
```

---

## Repository Structure

```text
shared-host/
├── include/
│   ├── shared_host.h              # Public C API header
│   └── internal/                  # Internal connection and mapping headers
├── src/
│   ├── shared_host_core.c         # Connection creation, attachment, and close logic
│   ├── shared_host_read.c         # Read operations (fast + slow)
│   ├── shared_host_write.c        # Write operations (fast + slow)
│   └── shm_operations/
│       └── shm_mapping.c          # OS shared memory mapping implementations
├── tests/
│   ├── test_utils.h               # Shared defines, structs, and inline utilities
│   ├── benchmark.c                # Server/client benchmark phases (latency, sweep, integrity)
│   ├── history.c                  # JSON persistence and historical regression comparison
│   └── main.c                     # Entry point and dual-mode orchestration
├── scripts/                       # Wrapper scripts (.bat, .cmd, .sh)
├── Makefile                       # Cross-platform GNU Makefile
└── README.md                      # Documentation and performance stats
```

---

## License

This library is licensed under the LGPL v3.0 License. See [LICENSE](LICENSE) for details.
