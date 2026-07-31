#include "internal/communication_model.h"
#include "internal/shm_mapping.h"
#include "shared_host.h"
#include <memoryapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

sh_result_t
create_shared_host_connection(const char *port,
                              shared_host_connection *out_connection) {
  if (port == NULL || out_connection == NULL) {
    return SH_ERR_INVALID_PARAMETER;
  }
  sh_result_t result = SH_OK;
  size_t size = 1 GB;
  HANDLE settingsBufferHandle = NULL;
  void *settingsBuffer = NULL;

  const char *settings_template = "Local\\shared_host_%s_settings";
  int settings_name_len = snprintf(NULL, 0, settings_template, port);
  char *settings_name = (char *)malloc(settings_name_len + 1);
  if (settings_name == NULL) {
    return SH_ERR_OOM;
  }
  sprintf(settings_name, settings_template, port);

  result = sh_create_shared_memory(
      settings_name,
      sizeof(shared_host_shared_settings_header) +
          sizeof(shared_host_shared_connection_header) * 2,
      &settingsBufferHandle, &settingsBuffer);
  free(settings_name);

  if (result != SH_OK) {
    return result;
  }

  shared_host_shared_settings_header *settings_header =
      (shared_host_shared_settings_header *)settingsBuffer;
  settings_header->size = size;

  HANDLE ownBufferHandle = NULL;
  void *ownBuffer = NULL;

  const char *own_buffer_template = "Local\\shared_host_%s_own_buffer";
  int own_buffer_name_len = snprintf(NULL, 0, own_buffer_template, port);
  char *own_buffer_name = (char *)malloc(own_buffer_name_len + 1);
  if (own_buffer_name == NULL) {
    UnmapViewOfFile(settingsBuffer);
    CloseHandle(settingsBufferHandle);
    return SH_ERR_OOM;
  }
  sprintf(own_buffer_name, own_buffer_template, port);

  result = sh_create_shared_memory(own_buffer_name, size, &ownBufferHandle,
                                   &ownBuffer);
  free(own_buffer_name);

  if (result != SH_OK) {
    UnmapViewOfFile(settingsBuffer);
    CloseHandle(settingsBufferHandle);
    return result;
  }

  HANDLE oppBufferHandle = NULL;
  void *oppBuffer = NULL;

  const char *opp_buffer_template = "Local\\shared_host_%s_opp_buffer";
  int opp_buffer_name_len = snprintf(NULL, 0, opp_buffer_template, port);
  char *opp_buffer_name = (char *)malloc(opp_buffer_name_len + 1);
  if (opp_buffer_name == NULL) {
    UnmapViewOfFile(settingsBuffer);
    CloseHandle(settingsBufferHandle);
    UnmapViewOfFile(ownBuffer);
    CloseHandle(ownBufferHandle);
    return SH_ERR_OOM;
  }
  sprintf(opp_buffer_name, opp_buffer_template, port);

  result = sh_create_shared_memory(opp_buffer_name, size, &oppBufferHandle,
                                   &oppBuffer);
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
  out_connection->own_shared_connection_header =
      (shared_host_shared_connection_header
           *)(((char *)settingsBuffer) +
              sizeof(shared_host_shared_settings_header));
  out_connection->opp_shared_connection_header =
      out_connection->own_shared_connection_header + 1;

  out_connection->own_page_start = ownBuffer;
  out_connection->opp_page_start = oppBuffer;

  out_connection->own_shared_connection_header->current_item_offset = 0;
  out_connection->own_shared_connection_header->last_item_offset = 0;

  write_to_shared_host_connection(out_connection, "hello", 6);

  return SH_OK;
}

sh_result_t
connect_to_shared_host_connection(const char *port, size_t *size,
                                  shared_host_connection *out_connection) {
  sh_result_t result = SH_OK;

  HANDLE settingsBufferHandle = NULL;
  void *settingsBuffer = NULL;

  const char *settings_template = "Local\\shared_host_%s_settings";
  int settings_name_len = snprintf(NULL, 0, settings_template, port);
  char *settings_name = (char *)malloc(settings_name_len + 1);
  if (settings_name == NULL) {
    return SH_ERR_OOM;
  }
  sprintf(settings_name, settings_template, port);

  *size = sizeof(shared_host_shared_settings_header) +
          sizeof(shared_host_shared_connection_header) * 2;

  result = sh_connect_to_shared_memory(settings_name, *size,
                                       &settingsBufferHandle, &settingsBuffer);
  free(settings_name);

  if (result != SH_OK) {
    return result;
  }

  shared_host_shared_settings_header *settings_header =
      (shared_host_shared_settings_header *)settingsBuffer;
  *size = settings_header->size;
  // settings_header->connection_type = ;

  HANDLE ownBufferHandle = NULL;
  void *ownBuffer = NULL;

  const char *own_buffer_template = "Local\\shared_host_%s_opp_buffer";
  int own_buffer_name_len = snprintf(NULL, 0, own_buffer_template, port);
  char *own_buffer_name = (char *)malloc(own_buffer_name_len + 1);
  if (own_buffer_name == NULL) {
    UnmapViewOfFile(settingsBuffer);
    CloseHandle(settingsBufferHandle);
    return SH_ERR_OOM;
  }
  sprintf(own_buffer_name, own_buffer_template, port);

  result = sh_connect_to_shared_memory(own_buffer_name, *size, &ownBufferHandle,
                                       &ownBuffer);
  free(own_buffer_name);

  if (result != SH_OK) {
    UnmapViewOfFile(settingsBuffer);
    CloseHandle(settingsBufferHandle);
    return result;
  }

  HANDLE oppBufferHandle = NULL;
  void *oppBuffer = NULL;

  const char *opp_buffer_template = "Local\\shared_host_%s_own_buffer";
  int opp_buffer_name_len = snprintf(NULL, 0, opp_buffer_template, port);
  char *opp_buffer_name = (char *)malloc(opp_buffer_name_len + 1);
  if (opp_buffer_name == NULL) {
    UnmapViewOfFile(settingsBuffer);
    CloseHandle(settingsBufferHandle);
    UnmapViewOfFile(ownBuffer);
    CloseHandle(ownBufferHandle);
    return SH_ERR_OOM;
  }
  sprintf(opp_buffer_name, opp_buffer_template, port);

  result = sh_connect_to_shared_memory(opp_buffer_name, *size, &oppBufferHandle,
                                       &oppBuffer);
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
  out_connection->opp_shared_connection_header =
      (shared_host_shared_connection_header
           *)(((char *)settingsBuffer) +
              sizeof(shared_host_shared_settings_header));
  out_connection->own_shared_connection_header =
      out_connection->opp_shared_connection_header + 1;

  out_connection->own_page_start = ownBuffer;
  out_connection->opp_page_start = oppBuffer;

  out_connection->own_shared_connection_header->current_item_offset = 0;
  out_connection->own_shared_connection_header->last_item_offset = 0;

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

  free(connection);

  return SH_OK;
}

sh_result_t write_to_shared_host_connection(shared_host_connection *connection,
                                            void *buffer, size_t buffer_size) {
  if (connection == NULL || buffer == NULL || buffer_size == 0) {
    return SH_ERR_INVALID_PARAMETER;
  }

  void *last_item_address =
      (void *)((char *)connection->opp_page_start +
               connection->opp_shared_connection_header
                   ->last_item_offset); // the address of the last item

  size_t ring_buffer_size =
      connection->shared_settings_page_ptr->size; // the ring buffer size

  size_t current_item_offset =
      *(size_t *)(last_item_address); // the address of the last item's next
                                      // item offset, aka our current item
                                      // offset's pointer

  if (current_item_offset + buffer_size + 2 * sizeof(size_t) >=
      ring_buffer_size) { // check if the offset of the the current item +
                          // buffer_size + 2*sizeof(size_t) is bigger than the
                          // ring buffer size
    // check if we can write from page_start
    if (connection->opp_shared_connection_header->current_item_offset >=
        buffer_size +
            2 * sizeof(size_t)) { // we can write from the start, aka check if
                                  // we can write the message inbetween 0 and
                                  // the reader's current read item
      *(size_t *)(last_item_address) = 0; // set the last item's next item to 0
      current_item_offset = 0;
    } else { // we cant write anything at all
      return SH_ERR_MESSAGE_TOO_LONG;
    }
  } else {
    if (current_item_offset <
            connection->opp_shared_connection_header->current_item_offset &&
        current_item_offset + buffer_size + 2 * sizeof(size_t) >=
            connection->opp_shared_connection_header->current_item_offset) {
      return SH_ERR_MESSAGE_TOO_LONG;
    }
  }

  void *current_item_address =
      (void *)((char *)connection->opp_page_start +
               current_item_offset); // this is the address for our current item
  *(size_t *)current_item_address =
      current_item_offset + buffer_size +
      2 * sizeof(size_t); // this sets the next item offset to the current item
                          // offset + buffer_size + 2*sizeof(size_t)
  *(size_t *)((char *)current_item_address + sizeof(size_t)) =
      buffer_size; // this sets the size of the current item

  memcpy((void *)((char *)current_item_address + 2 * sizeof(size_t)), buffer,
         buffer_size); // this copies the buffer to the current item address +
                       // 2*sizeof(size_t) for headers

#ifdef _WIN32
  MemoryBarrier();
#else
  __sync_synchronize();
#endif

  connection->opp_shared_connection_header->last_item_offset =
      current_item_offset;

  return SH_OK;
}

sh_result_t read_from_shared_host_connection(shared_host_connection *connection,
                                             void **buffer,
                                             size_t *buffer_size) {
  if (connection == NULL || buffer == NULL || buffer_size == NULL) {
    return SH_ERR_INVALID_PARAMETER;
  }

  while (connection->own_shared_connection_header->current_item_offset ==
         connection->own_shared_connection_header->last_item_offset) {
#ifdef _WIN32
    YieldProcessor();
#else
    __asm__ volatile("pause" ::: "memory");
#endif
  }

  void *current_item_address =
      (void *)((char *)connection->own_page_start +
               connection->own_shared_connection_header->current_item_offset);

  size_t next_item_offset = *(size_t *)(current_item_address);

  current_item_address =
      (void *)((char *)connection->own_page_start + next_item_offset);

  *buffer_size = *(size_t *)((char *)current_item_address + sizeof(size_t));
  *buffer = (void *)((char *)current_item_address + 2 * sizeof(size_t));

  // *buffer = malloc(*buffer_size);
  // if (*buffer == NULL) {
  //     return SH_ERR_OOM;
  // }
  // memcpy(*buffer, (void*)((char*)current_item_address + 2*sizeof(size_t)),
  // *buffer_size);

  connection->own_shared_connection_header->current_item_offset =
      next_item_offset;

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
