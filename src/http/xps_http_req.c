#include "../xps.h"
#include "xps_http.h"

int http_process_request_line(xps_http_req_t *http_req, xps_buffer_t *buff) {
	int error = xps_http_parse_request_line(http_req, buff);/*parse request line*/
  if (error != OK)
    return error;
  http_req->request_line = str_from_ptrs(http_req->request_line_start, http_req->request_line_end);
  /*similarly assign method, uri, schema, host, path, pathname*/
  http_req->method = str_from_ptrs(http_req->method_start, http_req->method_end);
  http_req->uri = str_from_ptrs(http_req->uri_start, http_req->uri_end);
  http_req->schema = str_from_ptrs(http_req->schema_start, http_req->schema_end);
  http_req->host = str_from_ptrs(http_req->host_start, http_req->host_end);
  http_req->path = str_from_ptrs(http_req->path_start, http_req->path_end);
  http_req->pathname = str_from_ptrs(http_req->pathname_start, http_req->pathname_end);
  http_req->http_version = str_from_ptrs(http_req->http_major, http_req->http_minor);
  http_req->port = -1;
  /*find port_str from port start and end pointers*/
  char * port_str = NULL;
  if(http_req->port_start!=NULL && http_req->port_end!=NULL)
  {
    str_from_ptrs(http_req->port_start, http_req->port_end);
  }
  if(port_str == NULL)
  {
    if(http_strcmp(http_req->schema, "http", 4))
    {
      http_req->port = 80;
    }
    else {
      http_req->port = 443;
    }
  }
  else {
    http_req->port = atoi(port_str);
  }
  /*if port_str is null assign default port number 80 for http and 443 for https
  if not null assign atoi(port_str)*/
  /*free port_str*/
  free(port_str);
  printf("REQUEST LINE: %s\n", http_req->request_line);
  printf("method: %s\n", http_req->method);
  printf("uri: %s\n", http_req->uri);
  printf("schema: %s\n", http_req->schema);
  printf("host: %s\n", http_req->host);
  printf("path: %s\n", http_req->path);
  printf("pathname: %s\n", http_req->pathname);
  printf("httpv: %s\n", http_req->http_version);
  printf("port: %d\n", http_req->port);
  return OK;
}

int http_process_headers(xps_http_req_t *http_req, xps_buffer_t *buff) {
  /*assert*/
  assert(http_req !=NULL);
  assert(buff != NULL);
  /*initialize headers list of http_req*/
  vec_init(&(http_req->headers));
  int error;
  while (1) {
    error = xps_http_parse_header_line(http_req, buff);/*parse header*/
    if (error == E_FAIL || error == E_AGAIN)
      break;
    if (error == OK || error == E_NEXT) {
      /* Alloc memory for new header*/
      xps_keyval_t * header = (xps_keyval_t *)malloc(sizeof(xps_keyval_t));
      /*assign key,val from their corresponding start and end pointers*/
      header->key = str_from_ptrs(http_req->header_key_start, http_req->header_key_end);
      header->val = str_from_ptrs(http_req->header_val_start, http_req->header_val_end);
      /*push this header into headers list of http_req*/
      vec_push(&(http_req->headers), header);
      /*if error is E_NEXT continue*/
      if(error == E_NEXT)
      {
        continue;
      }
    }
    printf("HEADERS\n");
      for (int i = 0; i < http_req->headers.length; i++) {
        xps_keyval_t *header = http_req->headers.data[i];
        /*print key and val*/
        printf("%s %s\n", header->key, header->val);
      }
		return OK;
    }
    /*error occurs, thus iterate through header list, free each header*/
    logger(LOG_DEBUG, "http_process_headers", "header error");
    for(int i=0; i<http_req->headers.length; i++)
    {
      xps_keyval_t *header = http_req->headers.data[i];
      if(header!=NULL)
      {
        http_req->headers.data[i] = NULL;
      }
    }
    /*deinitialize headers list*/
    vec_deinit(&(http_req->headers));
    return E_FAIL;
  }

xps_buffer_t *xps_http_req_serialize(xps_http_req_t *http_req) {
  assert(http_req != NULL);
  /* Serialize headers into a buffer headers_str*/
  xps_buffer_t *headers_str = xps_http_serialize_headers(&(http_req->headers));
  size_t final_len = strlen(http_req->request_line) + 1 + headers_str->len + 1;   /*Calculate length for final buffer*/
  /*Create instance for final buffer*/
  xps_buffer_t * buff = xps_buffer_create(DEFAULT_BUFFER_SIZE, 0, NULL);
  /*Copy everything to final buffer*/
  memcpy(buff->pos, http_req->request_line, strlen(http_req->request_line));
  buff->pos += strlen(http_req->request_line);
  /*similarly copy "\n", headers_str, "\n"*/
  memcpy(buff->pos, "\n", strlen("\n"));
  buff->pos++;
  memcpy(buff->pos, headers_str->data, strlen(headers_str->data));
  buff->pos += strlen(headers_str->data);
	/*destroy headers_str buffer*/
    xps_buffer_destroy(headers_str);
  return buff;
}

xps_http_req_t *xps_http_req_create(xps_core_t *core, xps_buffer_t *buff, int *error) {
	/*assert*/
    assert(core!=NULL);
    assert(buff!=NULL);
  *error = E_FAIL;
  printf("%.*s\n", (int)buff->len, buff->pos);
	/* Alloc memory for http_req instance*/
    xps_http_req_t * http_req = (xps_http_req_t *)malloc(sizeof(xps_http_req_t));
  memset(http_req, 0, sizeof(xps_http_req_t));
	/*Set initial parser state*/
    http_req->parser_state = RL_START;
  /*Process request line and handle possible errors*/
  if(http_process_request_line(http_req, buff) != OK)
  {
    logger(LOG_ERROR, "xps_http_req_create", "parse_request_line failed");
    return NULL;
  }
  /*Process headers and handle possible errors*/
  if(http_process_headers(http_req, buff) != OK)
  {
    logger(LOG_ERROR, "xps_http_req_create", "parse_header_line failed");
    return NULL;
  }
	// Header length
  http_req->header_len = (size_t)(buff->pos - buff->data);
	// Body length is retrieved from header Content-Length
  http_req->body_len = 0;
  const char *body_len_str = xps_http_get_header(&(http_req->headers), "Content-Length");/*get header value for key Content-Length*/
  if(body_len_str == NULL)
  {
    logger(LOG_ERROR, "xps_http_req_create", "body_len_str is NULL");
    http_req->body_len = 0;
  }
  else
  {
  /*assign body_len*/
    http_req->body_len = atoi(body_len_str);
  }
  *error = OK;
	return http_req;
}

void xps_http_req_destroy(xps_core_t *core, xps_http_req_t *http_req) {
  if(http_req == NULL)
  {
    return;
  }
  /*Frees memory allocated for various components of the HTTP request line(request line, method,etc)*/
  for(int i=0; i<http_req->headers.length; i++)
  {
      xps_keyval_t *header = http_req->headers.data[i];
      if(header!=NULL)
      {
        free(http_req->headers.data[i]);
      }
  }
vec_deinit(&(http_req->headers));
  /*iterate through the headers list of http_req and free the memory*/
  /*de-intialize the headers list*/
  /*free http_req*/
  free(http_req);
}