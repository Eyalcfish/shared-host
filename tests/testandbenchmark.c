#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <synchapi.h>
#include <windows.h>
#include <shared_host.h>
#include <string.h>
#include <stdint.h>
#include <intrin.h> // For _mm_pause()

#define STRESS_ITERATIONS 10000000 // 10 million operations
#define PAYLOAD_SIZE 64            // 64-byte payload (typical cache line size)
#define SH_OK 0                    // Assuming 0 is success based on your code

void server() {
    shared_host_connection* connection = (shared_host_connection*) malloc(sizeof(shared_host_connection));
    int error = create_shared_host_connection("test", connection);
    printf("Server created connection. Error: %s\n", error_to_string(error));

    void* buffer;
    size_t buffer_size;
    uint64_t data_errors = 0;

    // High-resolution Windows timers
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);

    printf("Server waiting for data...\n");

    // Read the first message to synchronize and start the timer.
    // This prevents timing the delay between starting the server and the client.
    read_from_shared_host_connection(connection, &buffer, &buffer_size);
    QueryPerformanceCounter(&start);

    for (uint64_t i = 1; i < STRESS_ITERATIONS; i++) {
        read_from_shared_host_connection(connection, &buffer, &buffer_size);

        // Verify data integrity without string parsing
        if (buffer_size >= sizeof(uint64_t)) {
            uint64_t seq = *(uint64_t*)buffer;
            if (seq != i) {
                data_errors++;
            }
        }
    }

    QueryPerformanceCounter(&end);

    // Calculate metrics
    double elapsed_seconds = (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    double ops_per_sec = (STRESS_ITERATIONS - 1) / elapsed_seconds;
    double mb_per_sec = ((STRESS_ITERATIONS - 1) * PAYLOAD_SIZE) / (1024.0 * 1024.0 * elapsed_seconds);

    printf("\n--- Stress Test Complete ---\n");
    printf("Total Time:     %.4f seconds\n", elapsed_seconds);
    printf("Throughput:     %.0f ops/sec\n", ops_per_sec);
    printf("Bandwidth:      %.2f MB/s\n", mb_per_sec);
    printf("Data Corrupts:  %llu\n", data_errors);

    // Cleanup/close logic here
}

void client() {
    shared_host_connection* connection = (shared_host_connection*) malloc(sizeof(shared_host_connection));
    size_t size = 0;
    int error = connect_to_shared_host_connection("test", &size, connection);
    printf("Client connected. Size: %zu, Error: %s\n", size, error_to_string(error));

    // Allocate on the stack. Bypasses heap contention completely.
    char payload[PAYLOAD_SIZE] = {0};

    for (uint64_t i = 0; i < STRESS_ITERATIONS; i++) {
        // Embed a sequence number directly into the first 8 bytes of the payload
        *(uint64_t*)payload = i;

        int write_err;
        // Replaces your `i--` logic. If the ring buffer is full, spin-wait.
        while ((write_err = write_to_shared_host_connection(connection, payload, PAYLOAD_SIZE)) != SH_OK) {
            // Emit a PAUSE instruction. This prevents branch misprediction penalties
            // when the loop exits and reduces power consumption during the spin-wait.
            _mm_pause();
        }
    }

    printf("Client finished writing %d messages.\n", STRESS_ITERATIONS);
    // print_1kb(connection->opp_page_start); // Optional inspection
}

// 1. Create a wrapper function that matches the Win32 ThreadProc signature
DWORD WINAPI server_wrapper(LPVOID _) {
    server();
    return 0; // Thread exit code
}

int main() {
    HANDLE hThread;
    DWORD threadId;

    // 2. Spawn the thread executing the server wrapper
    hThread = CreateThread(
        NULL,                   // Default security attributes
        0,                      // Default stack size (0 = use default)
        server_wrapper,         // Thread function pointer
        NULL,                   // Parameter to pass to the thread
        0,                      // Creation flags (0 = run immediately)
        &threadId               // Pointer to receive the thread identifier
    );

    if (hThread == NULL) {
        fprintf(stderr, "Thread creation failed. Error code: %lu\n", GetLastError());
        return 1;
    }

    Sleep(100);
    // 3. The main thread continues immediately to execute the client
    client();

    // 4. Wait for the server thread to terminate before exiting main
    // INFINITE blocks the main thread indefinitely until the server thread exits.
    WaitForSingleObject(hThread, INFINITE);

    // 5. Close the thread handle to prevent resource leaks
    CloseHandle(hThread);

    return 0;
}
