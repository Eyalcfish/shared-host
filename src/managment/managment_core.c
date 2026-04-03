#include "managment.h"

#include <stdatomic.h>

void broadcast_managment_message(shared_host_connection* connection, void* buffer, size_t buffer_size) {
    atomic_fetch_add(&connection->clients, 1); 
    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, "Global\\");

}       