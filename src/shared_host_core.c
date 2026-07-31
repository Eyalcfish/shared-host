#include "internal/communication_model.h"
#include "internal/shm_mapping.h"
#include "shared_host.h"
#include <memoryapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <wingdi.h>
#include <winnt.h>
#include <windows.h>
#endif

sh_result_t create_shared_host_connection(const char *port, char is_soft_locked, shared_host_connection *out_connection) {
	if (port == NULL || out_connection == NULL) {
		return SH_ERR_INVALID_PARAMETER;
	}
	sh_result_t result = SH_OK;
	size_t size = 1 GB;
	HANDLE settingsBufferHandle = NULL;
	void *settingsBuffer = NULL;

	// settings buffer

	char *settings_name;
	sh_result_t settings_name_result = format_unique_name(port, "settings", strlen(port) + strlen("settings"), &settings_name);

	if (settings_name_result != SH_OK) {
		return settings_name_result;
	}

	result = sh_create_shared_memory(settings_name, sizeof(shared_host_shared_settings_header) + sizeof(shared_host_shared_connection_header) * 2, &settingsBufferHandle, &settingsBuffer);
	free(settings_name);

	if (result != SH_OK) {
		return result;
	}

	shared_host_shared_settings_header *settings_header = (shared_host_shared_settings_header *)settingsBuffer;
	settings_header->size = size;
	settings_header->is_soft_locked = is_soft_locked;

	HANDLE ownBufferHandle = NULL;
	void *ownBuffer = NULL;

	// own buffer

	char *own_buffer_name;
	sh_result_t own_buffer_name_result = format_unique_name(port, "own_buffer", strlen(port) + strlen("own_buffer"), &own_buffer_name);
	if (own_buffer_name_result != SH_OK) {
		UnmapViewOfFile(settingsBuffer);
		CloseHandle(settingsBufferHandle);
		return own_buffer_name_result;
	}

	result = sh_create_shared_memory(own_buffer_name, size, &ownBufferHandle, &ownBuffer);
	free(own_buffer_name);

	if (result != SH_OK) {
		UnmapViewOfFile(settingsBuffer);
		CloseHandle(settingsBufferHandle);
		return result;
	}

	// opp buffer

	HANDLE oppBufferHandle = NULL;
	void *oppBuffer = NULL;

	char *opp_buffer_name;
	sh_result_t opp_buffer_name_result = format_unique_name(port, "opp_buffer", strlen(port) + strlen("opp_buffer"), &opp_buffer_name);
	if (opp_buffer_name_result != SH_OK) {
		UnmapViewOfFile(settingsBuffer);
		CloseHandle(settingsBufferHandle);
		UnmapViewOfFile(ownBuffer);
		CloseHandle(ownBufferHandle);
		return opp_buffer_name_result;
	}

	result = sh_create_shared_memory(opp_buffer_name, size, &oppBufferHandle, &oppBuffer);
	free(opp_buffer_name);

	if (result != SH_OK) {
		UnmapViewOfFile(settingsBuffer);
		CloseHandle(settingsBufferHandle);
		UnmapViewOfFile(ownBuffer);
		CloseHandle(ownBufferHandle);
		return result;
	}

	out_connection->shared_settings_page_handle = settingsBufferHandle;
	out_connection->own_shared_connection_buffer_handle = ownBufferHandle;
	out_connection->opp_shared_connection_buffer_handle = oppBufferHandle;

	out_connection->shared_settings_page_ptr = settingsBuffer;
	out_connection->own_shared_connection_header = (shared_host_shared_connection_header *)(((char *)settingsBuffer) + sizeof(shared_host_shared_settings_header));
	out_connection->opp_shared_connection_header = out_connection->own_shared_connection_header + 1;

	out_connection->own_page_start = ownBuffer;
	out_connection->opp_page_start = oppBuffer;

	out_connection->own_shared_connection_header->current_item_offset = 0;
	out_connection->own_shared_connection_header->last_item_offset = 0;

	// events

	char *own_event_name;
	sh_result_t own_event_name_result = format_unique_name(port, "own_event", strlen(port) + strlen("own_event"), &own_event_name);
	if (own_event_name_result != SH_OK) {
		return own_event_name_result;
	}

	char *opp_event_name;
	sh_result_t opp_event_name_result = format_unique_name(port, "opp_event", strlen(port) + strlen("opp_event"), &opp_event_name);
	if (opp_event_name_result != SH_OK) {
		free(own_event_name);
		return opp_event_name_result;
	}

	sh_result_t own_event_result = create_windows_event(own_event_name, &out_connection->own_event_handle);
	if (own_event_result != SH_OK) {
		free(own_event_name);
		free(opp_event_name);
		return own_event_result;
	}
	free(own_event_name);

	sh_result_t opp_event_result = create_windows_event(opp_event_name, &out_connection->opp_event_handle);
	if (opp_event_result != SH_OK) {
		free(own_event_name);
		free(opp_event_name);
		return opp_event_result;
	}
	free(opp_event_name);

	write_to_shared_host_connection(out_connection, "hello", 6);

	return SH_OK;
}

sh_result_t connect_to_shared_host_connection(const char *port, size_t *size, shared_host_connection *out_connection) {
	sh_result_t result = SH_OK;

	HANDLE settingsBufferHandle = NULL;
	void *settingsBuffer = NULL;

	char *settings_name;
	sh_result_t settings_name_result = format_unique_name(port, "settings", strlen(port) + strlen("settings"), &settings_name);
	if (settings_name_result != SH_OK) {
		return settings_name_result;
	}

	*size = sizeof(shared_host_shared_settings_header) + sizeof(shared_host_shared_connection_header) * 2;

	result = sh_connect_to_shared_memory(settings_name, *size, &settingsBufferHandle, &settingsBuffer);
	free(settings_name);

	if (result != SH_OK) {
		return result;
	}

	shared_host_shared_settings_header *settings_header = (shared_host_shared_settings_header *)settingsBuffer;
	*size = settings_header->size;

	HANDLE ownBufferHandle = NULL;
	void *ownBuffer = NULL;

	char *own_buffer_name;
	sh_result_t own_buffer_name_result = format_unique_name(port, "opp_buffer", strlen(port) + strlen("opp_buffer"), &own_buffer_name);
	if (own_buffer_name_result != SH_OK) {
		UnmapViewOfFile(settingsBuffer);
		CloseHandle(settingsBufferHandle);
		return own_buffer_name_result;
	}

	result = sh_connect_to_shared_memory(own_buffer_name, *size, &ownBufferHandle, &ownBuffer);
	free(own_buffer_name);

	if (result != SH_OK) {
		UnmapViewOfFile(settingsBuffer);
		CloseHandle(settingsBufferHandle);
		return result;
	}

	HANDLE oppBufferHandle = NULL;
	void *oppBuffer = NULL;

	char *opp_buffer_name = NULL;
	sh_result_t opp_buffer_name_result = format_unique_name(port, "own_buffer", strlen(port) + strlen("own_buffer"), &opp_buffer_name);
	if (opp_buffer_name_result != SH_OK) {
		UnmapViewOfFile(settingsBuffer);
		CloseHandle(settingsBufferHandle);
		UnmapViewOfFile(ownBuffer);
		CloseHandle(ownBufferHandle);
		return opp_buffer_name_result;
	}

	result = sh_connect_to_shared_memory(opp_buffer_name, *size, &oppBufferHandle, &oppBuffer);
	free(opp_buffer_name);

	if (result != SH_OK) {
		UnmapViewOfFile(settingsBuffer);
		CloseHandle(settingsBufferHandle);
		UnmapViewOfFile(ownBuffer);
		CloseHandle(ownBufferHandle);
		return result;
	}

	out_connection->shared_settings_page_handle = settingsBufferHandle;
	out_connection->own_shared_connection_buffer_handle = ownBufferHandle;
	out_connection->opp_shared_connection_buffer_handle = oppBufferHandle;

	out_connection->shared_settings_page_ptr = settingsBuffer;
	out_connection->opp_shared_connection_header = (shared_host_shared_connection_header *)(((char *)settingsBuffer) + sizeof(shared_host_shared_settings_header));
	out_connection->own_shared_connection_header = out_connection->opp_shared_connection_header + 1;

	out_connection->own_page_start = ownBuffer;
	out_connection->opp_page_start = oppBuffer;

	out_connection->own_shared_connection_header->current_item_offset = 0;
	out_connection->own_shared_connection_header->last_item_offset = 0;

	// events

	char *own_event_name;
	sh_result_t own_event_name_result = format_unique_name(port, "opp_event", strlen(port) + strlen("opp_event"), &own_event_name);
	if (own_event_name_result != SH_OK) {
		return own_event_name_result;
	}

	char *opp_event_name;
	sh_result_t opp_event_name_result = format_unique_name(port, "own_event", strlen(port) + strlen("own_event"), &opp_event_name);
	if (opp_event_name_result != SH_OK) {
		free(own_event_name);
		return opp_event_name_result;
	}

	sh_result_t own_event_result = connect_to_windows_event(opp_event_name, &out_connection->opp_event_handle);
	if (own_event_result != SH_OK) {
		free(own_event_name);
		free(opp_event_name);
		return own_event_result;
	}
	free(opp_event_name);

	sh_result_t opp_event_result = connect_to_windows_event(own_event_name, &out_connection->own_event_handle);
	if (opp_event_result != SH_OK) {
		free(own_event_name);
		return opp_event_result;
	}

	free(own_event_name);

	write_to_shared_host_connection(out_connection, "hello", 6);

	return SH_OK;
}

sh_result_t close_shared_host_connection(shared_host_connection *connection) {
	if (connection == NULL) {
		return SH_ERR_CONNECTION_CLOSED;
	}

	UnmapViewOfFile(connection->own_page_start);
	UnmapViewOfFile(connection->opp_page_start);
	UnmapViewOfFile(connection->shared_settings_page_ptr);
	CloseHandle(connection->shared_settings_page_handle);
	CloseHandle(connection->own_shared_connection_buffer_handle);
	CloseHandle(connection->opp_shared_connection_buffer_handle);
	CloseHandle(connection->own_event_handle);
	CloseHandle(connection->opp_event_handle);

	free(connection);

	return SH_OK;
}

sh_result_t write_to_shared_host_connection(shared_host_connection *connection, void *buffer, size_t buffer_size) {
	if (connection == NULL || buffer == NULL || buffer_size == 0) {
		return SH_ERR_INVALID_PARAMETER;
	}

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

	if (connection->shared_settings_page_ptr->is_soft_locked) {
    #ifdef _WIN32
    	SetEvent(connection->opp_event_handle);
    #endif
	}

	return SH_OK;
}

sh_result_t read_from_shared_host_connection(shared_host_connection *connection, void **buffer, size_t *buffer_size) {
	if (connection == NULL || buffer == NULL || buffer_size == NULL) {
		return SH_ERR_INVALID_PARAMETER;
	}

	while (connection->own_shared_connection_header->current_item_offset == connection->own_shared_connection_header->last_item_offset) {
#ifdef _WIN32
		YieldProcessor();
		if (connection->shared_settings_page_ptr->is_soft_locked) {
    		WaitForSingleObject(connection->own_event_handle, INFINITE);
    		ResetEvent(connection->own_event_handle);
		}
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

char *error_to_string(sh_result_t result) {
	switch (result) {
	case SH_OK:
		return "SH_OK";
	case SH_ERR_INVALID_PARAMETER:
		return "SH_ERR_INVALID_PARAMETER";
	case SH_ERR_OOM:
		return "SH_ERR_OOM";
	case SH_ERR_INVALID_PORT:
		return "SH_ERR_INVALID_PORT";
	case SH_ERR_MESSAGE_TOO_LONG:
		return "SH_ERR_MESSAGE_TOO_LONG";
	case SH_ERR_CONNECTION_CLOSED:
		return "SH_ERR_CONNECTION_CLOSED";
	case SH_ERR_CONNECTION_OWNED:
		return "SH_ERR_CONNECTION_OWNED";
	case SH_ERR_CONNECTION_NOT_OWNED:
		return "SH_ERR_CONNECTION_NOT_OWNED";
	case SH_ERR_UNKNOWN:
		return "SH_ERR_UNKNOWN";
	default:
		return "Unknown error";
	}
}
