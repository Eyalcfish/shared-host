#include <assert.h>
#include "shared_host.h"
#include <stdio.h>

int main()
{
    printf("Starting unit tests...\n");

    shared_host_connection *conn = NULL;
    sh_result_t res = create_shared_host_connection("test_port", 1024, &conn);
    assert(res == SH_OK);

    shared_host_connection *conn_client = NULL;
    res = connect_to_shared_host_connection("test_port", &conn_client);
    assert(res == SH_OK);

    res = close_shared_host_connection(conn_client);
    assert(res == SH_OK);

    res = close_shared_host_connection(conn);
    assert(res == SH_OK);

    printf("All tests passed.\n");
    return 0;
}
