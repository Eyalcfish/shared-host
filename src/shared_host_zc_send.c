#include <shared_host.h>
#include <string.h>

sh_result_t zc_send_to_shared_host_connection(shared_host_connection *connection) {
   	if (connection == NULL) {
		return SH_ERR_INVALID_PARAMETER;
	}

    return connection->send(connection);
}

// SH_FAST_CONNECTION
sh_result_t zc_send_to_shared_host_connection_fast(shared_host_connection *connection) {
	connection->opp_shared_connection_header->last_item_offset = connection->open_offset;

	return SH_OK;
}



// SH_SLOW_CONNECTION
sh_result_t zc_send_to_shared_host_connection_slow(shared_host_connection *connection) {
	connection->opp_shared_connection_header->last_item_offset = connection->open_offset;

    #ifdef _WIN32
   	SetEvent(connection->opp_event_handle);
    #endif

	return SH_OK;
}
