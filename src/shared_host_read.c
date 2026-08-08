#include <shared_host.h>

sh_result_t read_from_shared_host_connection(shared_host_connection *connection, void **buffer, size_t *buffer_size) {
    return connection->read(connection, buffer, buffer_size);
}

// SH_FAST_CONNECTION
sh_result_t read_from_shared_host_connection_fast(shared_host_connection *connection, void **buffer, size_t *buffer_size) {
	if (connection == NULL || buffer == NULL || buffer_size == NULL) {
		return SH_ERR_INVALID_PARAMETER;
	}

	while (connection->own_shared_connection_header->current_item_offset == connection->own_shared_connection_header->last_item_offset) {
#ifdef _WIN32
		YieldProcessor();
#else
		__asm__ volatile("pause" ::: "memory");
#endif
	}

	void *current_item_address = (void *)((char *)connection->own_page_start + connection->own_shared_connection_header->current_item_offset);

	size_t next_item_offset = *(size_t *)(current_item_address);

	current_item_address = (void *)((char *)connection->own_page_start + next_item_offset);

	*buffer_size = *(size_t *)((char *)current_item_address + sizeof(size_t));
	*buffer = (void *)((char *)current_item_address + 2 * sizeof(size_t));

	// *buffer = malloc(*buffer_size);
	// if (*buffer == NULL) {
	//     return SH_ERR_OOM;
	// }
	// memcpy(*buffer, (void*)((char*)current_item_address + 2*sizeof(size_t)),
	// *buffer_size);

	connection->own_shared_connection_header->current_item_offset = next_item_offset;

	return SH_OK;
}

// SH_SLOW_CONNECTION
sh_result_t read_from_shared_host_connection_slow(shared_host_connection *connection, void **buffer, size_t *buffer_size) {
	if (connection == NULL || buffer == NULL || buffer_size == NULL) {
		return SH_ERR_INVALID_PARAMETER;
	}

	if (connection->own_shared_connection_header->current_item_offset == connection->own_shared_connection_header->last_item_offset) {
#ifdef _WIN32
  		WaitForSingleObject(connection->own_event_handle, INFINITE);
  #endif
	}

	void *current_item_address = (void *)((char *)connection->own_page_start + connection->own_shared_connection_header->current_item_offset);

	size_t next_item_offset = *(size_t *)(current_item_address);

	current_item_address = (void *)((char *)connection->own_page_start + next_item_offset);

	*buffer_size = *(size_t *)((char *)current_item_address + sizeof(size_t));
	*buffer = (void *)((char *)current_item_address + 2 * sizeof(size_t));

	// *buffer = malloc(*buffer_size);
	// if (*buffer == NULL) {
	//     return SH_ERR_OOM;
	// }
	// memcpy(*buffer, (void*)((char*)current_item_address + 2*sizeof(size_t)),
	// *buffer_size);

	connection->own_shared_connection_header->current_item_offset = next_item_offset;

	return SH_OK;
}
