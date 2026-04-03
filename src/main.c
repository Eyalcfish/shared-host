#include <shared_host.h>
#include <stdio.h>

int main() {
    shared_host_connection* connection_server = NULL;
    sh_result_t result = create_shared_host_connection("test_port", 1024, &connection_server);

    if (result != SH_OK) {
        printf("Failed to create shared host connection: %d\n", result);
        return 1;
    }

    printf("Shared host connection created successfully on port: %s\n", connection_server->port);

    shared_host_connection* connection_client = NULL;
    result = connect_to_shared_host_connection("test_port", &connection_client);

    void* buffer = NULL;
    result = claim_ownership_of_shared_host_connection(connection_client, &buffer);

    if (result != SH_OK) {
        printf("Failed to claim ownership of shared host connection: %d\n", result);
        return 1;
    }

    strcpy((char*)buffer, "Hello from client!");
    result = send_package_to_shared_host_connection(connection_client);

    if (result != SH_OK) {
        printf("Failed to send package to shared host connection: %d\n", result);
        return 1;
    }

    printf("Package sent successfully from client to server.\n");

    buffer = NULL;
    result = claim_ownership_of_shared_host_connection(connection_server, &buffer);

    if (result != SH_OK) {
        printf("Failed to claim ownership of shared host connection: %d\n", result);
        return 1;
    }

    printf("Server received package: %s\n", (char*)buffer);

    result = lose_ownership_of_shared_host_connection(connection_server);

    if (result != SH_OK) {
        printf("Failed to lose ownership of shared host connection: %d\n", result);
        return 1;
    }

    result = clear_shared_host_connection(connection_server);

    if (result != SH_OK) {
        printf("Failed to clear shared host connection: %d\n", result);
        return 1;
    }

    // Clean up
    close_shared_host_connection(connection_server);
    free(connection_server);
    close_shared_host_connection(connection_client);
    free(connection_client);

    return 0;
}