<p align="center">
  <img src="assets/logo.png" alt="Shared-Host Logo" width="600"/>
</p>

# shared-host

A high-performance, ultra-low-latency Inter-Process Communication (IPC) library written in C. `shared-host` leverages shared memory ring buffers for fast, reliable message passing between processes.

Basically a faster localhost-like communication method.
g it
---

## Ai Usage Guidelines

- Ai in this project was only used for documentation testing and external tools.
- Everything in the `src` and `include` folders was written by me without ai assistance.

---

## Performance Statistics

```text
=================================================================
     STANDARD (write) vs ZERO-COPY (zc_write/zc_send) [FAST]       
=================================================================

--- Phase 1: Latency ---
 Metric          |     STANDARD |    ZERO-COPY |     Delta
------------------+--------------+--------------+-----------
 Avg Latency     |    171.7 ns |    154.2 ns |  -10.2%
 P99 Latency     |    900.0 ns |   1100.0 ns |  +22.2%

--- Phase 2: Throughput Sweep ---
 Payload |   STANDARD BW |  ZERO-COPY BW | Throughput Gain
---------+---------------+---------------+----------------
     64B |   803.2 MB/s |  1862.2 MB/s |     +131.8%
    256B |  1346.7 MB/s |  2138.7 MB/s |      +58.8%
   1024B |  1893.8 MB/s |  2911.3 MB/s |      +53.7%
   4096B |  5577.9 MB/s |  7172.4 MB/s |      +28.6%
  16384B | 10835.7 MB/s | 13388.3 MB/s |      +23.6%
  65536B | 10807.0 MB/s | 13116.6 MB/s |      +21.4%

--- Phase 3: Integrity ---
 STANDARD: PASSED  |  ZERO-COPY: PASSED
```

### Performance Summary

| Metric / Payload | Standard `write` | Zero-Copy `zc_write` / `zc_send` |
| :--- | :--- | :--- |
| **Average Latency (Phase 1)** | 171.7 ns (P50: 0.0 ns, P99: 900.0 ns) | **154.2 ns** (P50: 0.0 ns, P99: 1100.0 ns) |
| **Peak Throughput (64B)** | 13,159,522 ops/sec (76.0 ns avg) | **30,509,943 ops/sec** (32.8 ns avg) |
| **Peak Bandwidth (16KB - 64KB)** | ~10.8 GB/s Payload | **>13.5 GB/s** Payload |
| **Data Integrity (Phase 3)** | **100% Valid** (0 corruptions) | **100% Valid** (0 corruptions) |

---

## Features

- **Ultra-Low Latency**: Sub-microsecond message delivery (~154.2 ns avg latency with zero-copy).
- **High Throughput**: Exceeds **30 Million ops/sec** on 64B payloads and **13.5 GB/s** bandwidth on large payloads using zero-copy mode.
- **Zero-Copy API**: Avoids extra memory copies by reserving buffers directly within the mapped shared memory ring buffer (`zc_write_to_shared_host_connection` & `zc_send_to_shared_host_connection`).
- **Flexible Connection Modes**: Supports `SH_FAST_CONNECTION` (spin-polling for ultra-low latency) and `SH_SLOW_CONNECTION` (OS event synchronization for low CPU utilization).
- **Zero Corruption Guarantee**: Includes boundary wrap validation and sequence tracking.
- **Clean C API**: Host connection creation, connection attachment, standard read/write operations, zero-copy operations, and resource cleanup.
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

---

### Zero-Copy API

For maximum performance on medium to large payloads, use the zero-copy API to write directly into shared memory without intermediate `memcpy` operations:

1. **`zc_write_to_shared_host_connection(connection, &buffer, buffer_size)`**:
   Reserves `buffer_size` bytes in the shared memory ring buffer and sets `*buffer` to point directly to the destination memory.
2. **`zc_send_to_shared_host_connection(connection)`**:
   Publishes the reserved buffer to the receiver and signals connection synchronization (if using `SH_SLOW_CONNECTION`).

---

### Code Examples

#### Standard Write & Read Example

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

#### Zero-Copy Write & Send Example

```c
#include <shared_host.h>

void *tx_buffer = NULL;
size_t payload_size = 4096;

// 1. Reserve zero-copy buffer space directly in shared memory
if (zc_write_to_shared_host_connection(&client_conn, &tx_buffer, payload_size) == SH_OK) {
    // 2. Populate payload directly in tx_buffer (zero memcpy overhead!)
    snprintf((char*)tx_buffer, payload_size, "Direct zero-copy payload");

    // 3. Publish and send to receiver
    zc_send_to_shared_host_connection(&client_conn);
}
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
│   ├── shared_host_write.c        # Standard write operations (fast + slow)
│   ├── shared_host_zc_write.c     # Zero-copy buffer reservation logic
│   ├── shared_host_zc_send.c      # Zero-copy message publish/signal logic
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
