#include "xps_http_res.h"
#include "xps_http.h"

char* get_http_timestamp() {
    // 30 bytes is exactly enough for "Wed, 21 Oct 2015 07:28:00 GMT\0"
    size_t buffer_size = 30; 
    char *buffer = malloc(buffer_size*sizeof(char));
    
    if (buffer == NULL) {
        return NULL;
    }

    time_t raw_time;
    time(&raw_time);
    struct tm *gmt_time = gmtime(&raw_time);

    strftime(buffer, buffer_size, "%a, %d %b %Y %H:%M:%S GMT", gmt_time);

    return buffer;
}

xps_http_res_t *xps_http_res_create(xps_core_t *core, u_int code)
{
  xps_http_res_t *http_res = (xps_http_res_t*)malloc(sizeof(xps_http_res_t)/* fill this */);
  if (http_res == NULL) {
    logger(LOG_ERROR, "xps_http_res_create()",
           "failed to alloc memory for http_res. malloc() returned NULL");
    return NULL;
  }

  if(code == 200)
  {
    sprintf(http_res->response_line, "HTTP/1.1 200 OK");
  }
  else if(code == HTTP_BAD_REQUEST)
  {
    sprintf(http_res->response_line, "HTTP/1.1 400 Bad Request");
  }
  else if(code == HTTP_NOT_FOUND)
  {
    sprintf(http_res->response_line, "HTTP/1.1 404 Not Found");
  }
  else if(code == HTTP_OK)
  {
    sprintf(http_res->response_line,"HTTP/1.1 200 OK");
  }
  else if(code == HTTP_MOVED_TEMPORARILY)
  {
    sprintf(http_res->response_line,"HTTP/1.1 302 Found");
  }
  else if(code == HTTP_INTERNAL_SERVER_ERROR)
  {
    sprintf(http_res->response_line,"HTTP/1.1 500 Internal Server Error");
  }
  else if(code == HTTP_FORBIDDEN)
  {
    sprintf(http_res->response_line,"HTTP/1.1 403 Forbidden");
  }
  vec_init(&(http_res->headers));
  http_res->body = xps_buffer_create(BUFF_SIZE, 0, NULL);
  xps_http_set_header(&(http_res->headers), "Date", get_http_timestamp());
  xps_http_set_header(&(http_res->headers), "Server", SERVER_NAME);
  xps_http_set_header(&(http_res->headers), "Access-Control-Allow-Origin", "*");

  logger(LOG_DEBUG, "xps_http_res_create()",
           "http_res created");
  return http_res;
}

void xps_http_res_destroy(xps_http_res_t *res)
{
  assert(res!=NULL);
    for(int i=0; i<res->headers.length; i++)
  {
      xps_keyval_t *header = res->headers.data[i];
      if(header!=NULL)
      {
        free(header->key);
        free(header->val);
        free(header);
      }
  }
  vec_deinit(&(res->headers));
  xps_buffer_destroy(res->body);
  free(res);
}

xps_buffer_t *xps_http_res_serialize(xps_http_res_t *http_res) {
  /* valid params */
  assert(http_res!=NULL);

  // Serialize headers
  xps_buffer_t *headers_str =  xps_http_serialize_headers(&(http_res->headers))/* fill this */;
  if (headers_str == NULL) {
    logger(LOG_ERROR, "xps_http_res_serialize()", "failed to serialize headers");
    return NULL;
  }

  // Calculate length for final buffer
  size_t final_len = strlen(http_res->response_line) + 1 + headers_str->len + http_res->body->len +  1;/* fill this */;

  // Create instance for final buffer
  xps_buffer_t *buff = xps_buffer_create(BUFF_SIZE, 0, NULL)/* fill this */;
  if (buff == NULL) {
    logger(LOG_ERROR, "xps_http_res_serialize()", "failed to create buffer instance");
    xps_buffer_destroy(headers_str);
    return NULL;
  }

  // Copy everything
  /* copy response line */
  memcpy(buff->pos, http_res->response_line, strlen(http_res->response_line));
  buff->pos += strlen(http_res->response_line);

  memcpy(buff->pos, "\r\n", strlen("\r\n"));
  buff->pos += 2;

  /* copy headers */
  memcpy(buff->pos, headers_str->data, headers_str->len);
  buff->pos += headers_str->len;
  memcpy(buff->pos, "\r\n", strlen("\r\n"));
  buff->pos += 2;

  /* copy response body*/
  if(http_res->body->len > 0)
  {
    memcpy(buff->pos, http_res->body->data, http_res->body->len);
    buff->pos += http_res->body->len;
    memcpy(buff->pos, "\r\n", strlen("\r\n"));
    buff->pos += 2;
  }

  buff->len = buff->pos - buff->data;
  xps_buffer_destroy(headers_str);

  return buff;
}