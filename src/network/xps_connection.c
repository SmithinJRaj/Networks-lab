#include "xps_connection.h"
#include <sys/epoll.h>

void connection_read_handler(void *ptr);
void connection_write_handler(void *ptr);
void connection_loop_read_handler(void *ptr);
void connection_loop_write_handler(void *ptr);
void connection_loop_close_handler(void *ptr);

void strrev(u_char * s)
{
    char temp;
    for(int i = 0, j = strlen((const char*)s)-2; i<j; i++,j--)
    {
        temp = s[i];
        s[i] = s[j];
        s[j] = temp;
    }
}

xps_connection_t *xps_connection_create(xps_core_t *core, int sock_fd) {

  xps_connection_t *connection = malloc(sizeof(xps_connection_t));
  if (connection == NULL) {
    logger(LOG_ERROR, "xps_connection_create()", "malloc() failed for 'connection'");
    return NULL;
  }

  xps_buffer_list_t *buffer_list = xps_buffer_list_create();
  if (buffer_list == NULL) {
    logger(LOG_ERROR, "xps_connection_create()", "malloc() failed for 'buffer_list'");
    return NULL;
  }

  // Init values
  connection->core = core;
  connection->sock_fd = sock_fd;
  connection->listener = NULL;
  connection->remote_ip = get_remote_ip(sock_fd);
  connection->write_buff_list = buffer_list;
  connection->write_buff_list->len = 0;
  connection->read_ready = false;
  connection->write_ready = false;
  connection->send_handler = connection_write_handler;
  connection->recv_handler = connection_read_handler;

  xps_loop_attach(core->loop, sock_fd, EPOLLIN | EPOLLOUT|EPOLLET, connection, connection_loop_read_handler, connection_loop_write_handler, connection_loop_close_handler);

  vec_push(&core->connections, connection);

  logger(LOG_DEBUG, "xps_connection_create()", "created connection");
  return connection;

}

void xps_connection_destroy(xps_connection_t *connection) {

  /* validate params */
  assert(connection != NULL);

  /* set connection to NULL in 'connections' list */
  for(int i =0; i<connection->core->connections.length; i++)
  {
    xps_connection_t *curr = connection->core->connections.data[i];
    if(curr == connection)
    {
        connection->core->connections.data[i] = NULL;
        break;
    }
  }

  /* detach connection from loop */
  xps_loop_detach(connection->core->loop, connection->sock_fd);

  /* close connection socket FD */
  close(connection->sock_fd);

  /* free connection->remote_ip */
  free(connection->remote_ip);

  xps_buffer_list_destroy(connection->write_buff_list);

  /* free connection instance */
  free(connection);

  logger(LOG_DEBUG, "xps_connection_destroy()", "destroyed connection");

}

void connection_loop_read_handler(void* ptr) {
    assert(ptr != NULL);
    /*set read_ready flag to true*/
	  xps_connection_t *connection = ptr;
    connection->read_ready = true;
}

void connection_loop_write_handler(void* ptr) {
    assert(ptr != NULL);
    /*set write_ready flag to true*/
    xps_connection_t *connection = ptr;
    connection->write_ready = true;
}

void connection_read_handler(void *ptr) {

  /* validate params */
  assert(ptr != NULL);
  xps_connection_t *connection = ptr;

  u_char buff[BUFF_SIZE];
  memset(buff, 0, BUFF_SIZE);
  long read_n = recv(connection->sock_fd, buff, sizeof(buff)-1, 0);

  if (read_n < 0) {
    logger(LOG_DEBUG, "xps_connection_read_handler()", "recv() failed");
    if(errno == EAGAIN || errno == EWOULDBLOCK)
    {
      connection->read_ready = false;
      return;
    }
    else {
      perror("Error message");
      xps_connection_destroy(connection);
      return;
    }
    return;
  }

  if (read_n == 0) {
    logger(LOG_INFO, "connection_read_handler()", "peer closed connection");
    xps_connection_destroy(connection);
    return;
  }

  buff[read_n] = '\0';
  
  /* print client message */
  printf("[CLIENT MESSAGE] %s", buff);

  /* reverse client message */
  strrev(buff);

  xps_buffer_t *buffer = xps_buffer_create(sizeof(buff), read_n, NULL);
  if (buffer != NULL) {
    memcpy(buffer->data, buff, read_n);
    xps_buffer_list_append(connection->write_buff_list, buffer);
  } else {
    logger(LOG_ERROR, "connection_loop_read_handler()", "Failed to allocate buffer");
  }
}

void connection_write_handler(void *ptr){
  assert(ptr != NULL);
  xps_connection_t *connection = ptr;

  if(connection->write_buff_list->len == 0)
  {
    logger(LOG_DEBUG, "connection_loop_write_handler()", "buffer_empty");
    return;
  }

  xps_buffer_t *buffer = xps_buffer_list_read(connection->write_buff_list, connection->write_buff_list->len);

  if(buffer == NULL)
  {
    logger(LOG_ERROR, "connection_loop_write_handler()", "failed to create buffer");
    return;
  }

  // Sending reversed message to client
  size_t bytes_written = 0;
  while (bytes_written < connection->write_buff_list->len) {
    long write_n = send(connection->sock_fd, buffer->data + bytes_written, connection->write_buff_list->len - bytes_written, 0);
    if (write_n < 0) {
      if(errno == EAGAIN || errno == EWOULDBLOCK)
      {
        logger(LOG_DEBUG, "connection_loop_write_handler()", "break because of eagain");
        connection->write_ready = false;
        break;
      }
      else {
        logger(LOG_ERROR, "xps_connection_write_handler()", "send() failed");
        perror("Error message");
        xps_buffer_destroy(buffer);
        xps_connection_destroy(connection);
        return;
      }
    }
    bytes_written+= write_n;
  }

  if(bytes_written > 0)
  {
    if(xps_buffer_list_clear(connection->write_buff_list, bytes_written) != OK)
    {
      logger(LOG_ERROR, "connection_loop_write_handler()", "Failed to clear buffer list");
    }
  }

  xps_buffer_destroy(buffer);
}

void connection_loop_close_handler(void *ptr){
  xps_connection_t *connection = ptr;
  xps_connection_destroy(connection);
  return;
}