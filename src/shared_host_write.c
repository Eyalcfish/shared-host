#include <shared_host.h>
#include <string.h>

sh_result_t write_to_shared_host_connection(shared_host_connection *connection, void *buffer, size_t buffer_size) {
   	if (connection == NULL || buffer == NULL || buffer_size == 0) {
		return SH_ERR_INVALID_PARAMETER;
	}

    return connection->write(connection, buffer, buffer_size);
}

// SH_FAST_CONNECTION
sh_result_t write_to_shared_host_connection_fast(shared_host_connection *connection, void *buffer, size_t buffer_size) {
	void *last_item_address = (void *)((char *)connection->opp_page_start + connection->opp_shared_connection_header->last_item_offset); // the address of the last item

	size_t ring_buffer_size = connection->shared_settings_page_ptr->size; // the ring buffer size

	size_t current_item_offset = *(size_t *)(last_item_address); // the address of the last item's next
																 // item offset, aka our current item
																 // offset's pointer

	if (current_item_offset + buffer_size + 2 * sizeof(size_t) >= ring_buffer_size) { // check if the offset of the the current item +
																					  // buffer_size + 2*sizeof(size_t) is bigger than the
																					  // ring buffer size
		// check if we can write from page_start
		if (connection->opp_shared_connection_header->current_item_offset >= buffer_size + 2 * sizeof(size_t)) { // we can write from the start, aka check if
																												 // we can write the message inbetween 0 and
																												 // the reader's current read item
			*(size_t *)(last_item_address) = 0;																	 // set the last item's next item to 0
			current_item_offset = 0;
		} else { // we cant write anything at all
			return SH_ERR_MESSAGE_TOO_LONG;
		}
	} else {
		if (current_item_offset < connection->opp_shared_connection_header->current_item_offset && current_item_offset + buffer_size + 2 * sizeof(size_t) >= connection->opp_shared_connection_header->current_item_offset) {
			return SH_ERR_MESSAGE_TOO_LONG;
		}
	}

	void *current_item_address = (void *)((char *)connection->opp_page_start + current_item_offset); // this is the address for our current item
	*(size_t *)current_item_address = current_item_offset + buffer_size + 2 * sizeof(size_t);		 // this sets the next item offset to the current item
																									 // offset + buffer_size + 2*sizeof(size_t)
	*(size_t *)((char *)current_item_address + sizeof(size_t)) = buffer_size;						 // this sets the size of the current item

	memcpy((void *)((char *)current_item_address + 2 * sizeof(size_t)), buffer,
		   buffer_size); // this copies the buffer to the current item address +
						 // 2*sizeof(size_t) for headers

#ifdef _WIN32
	MemoryBarrier();
#else
	__sync_synchronize();
#endif

	connection->opp_shared_connection_header->last_item_offset = current_item_offset;

	return SH_OK;
}



// SH_SLOW_CONNECTION
sh_result_t write_to_shared_host_connection_slow(shared_host_connection *connection, void *buffer, size_t buffer_size) {
	void *last_item_address = (void *)((char *)connection->opp_page_start + connection->opp_shared_connection_header->last_item_offset); // the address of the last item

	size_t ring_buffer_size = connection->shared_settings_page_ptr->size; // the ring buffer size

	size_t current_item_offset = *(size_t *)(last_item_address); // the address of the last item's next
																 // item offset, aka our current item
																 // offset's pointer

	if (current_item_offset + buffer_size + 2 * sizeof(size_t) >= ring_buffer_size) { // check if the offset of the the current item +
																					  // buffer_size + 2*sizeof(size_t) is bigger than the
																					  // ring buffer size
		// check if we can write from page_start
		if (connection->opp_shared_connection_header->current_item_offset >= buffer_size + 2 * sizeof(size_t)) { // we can write from the start, aka check if
																												 // we can write the message inbetween 0 and
																												 // the reader's current read item
			*(size_t *)(last_item_address) = 0;																	 // set the last item's next item to 0
			current_item_offset = 0;
		} else { // we cant write anything at all
			return SH_ERR_MESSAGE_TOO_LONG;
		}
	} else {
		if (current_item_offset < connection->opp_shared_connection_header->current_item_offset && current_item_offset + buffer_size + 2 * sizeof(size_t) >= connection->opp_shared_connection_header->current_item_offset) {
			return SH_ERR_MESSAGE_TOO_LONG;
		}
	}

	void *current_item_address = (void *)((char *)connection->opp_page_start + current_item_offset); // this is the address for our current item
	*(size_t *)current_item_address = current_item_offset + buffer_size + 2 * sizeof(size_t);		 // this sets the next item offset to the current item
																									 // offset + buffer_size + 2*sizeof(size_t)
	*(size_t *)((char *)current_item_address + sizeof(size_t)) = buffer_size;						 // this sets the size of the current item

	memcpy((void *)((char *)current_item_address + 2 * sizeof(size_t)), buffer,
		   buffer_size); // this copies the buffer to the current item address +
						 // 2*sizeof(size_t) for headers

#ifdef _WIN32
	MemoryBarrier();
#else
	__sync_synchronize();
#endif

	connection->opp_shared_connection_header->last_item_offset = current_item_offset;

    #ifdef _WIN32
   	SetEvent(connection->opp_event_handle);
    #endif

	return SH_OK;
}
