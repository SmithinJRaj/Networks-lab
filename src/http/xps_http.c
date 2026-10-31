#include "../xps.h"

int xps_http_parse_request_line(xps_http_req_t *http_req, xps_buffer_t *buff) {
  /*get current parser state*/
  xps_http_parser_state_t parser_state = http_req->parser_state; 
  u_char *p_ch = buff->pos;//current buffer postion

  for (u_char *p = p_ch; *(p)!='\0'; p++) {
    char ch = *p;
    switch (parser_state) {
      case RL_START:
        /*assign request_line_start, ignore CR and LF, fail if not upper case, assign method_start*/
        http_req->request_line_start = p;
        if(ch == '\r' || ch == '\n')
        {
          break;
        }
        if(ch < 'A' || ch > 'Z')
        {
          return E_FAIL;
        }
        http_req->method_start = p;
        parser_state = RL_METHOD;
        break;

      case RL_METHOD:
        if (ch == ' ') {
          size_t method_len = p - http_req->method_start;
          http_req->method_n = E_FAIL;
          if (method_len == 3 && http_strcmp(http_req->method_start, "GET", 3))
            http_req->method_n = HTTP_GET;
          /*similarly check for other methods PUT, HEAD, etc*/
          if (method_len == 4 && http_strcmp(http_req->method_start, "POST", 4))
            http_req->method_n = HTTP_POST;
          if (method_len == 4 && http_strcmp(http_req->method_start, "HEAD", 4))
            http_req->method_n = HTTP_HEAD;
          /*assign method_end*/
          if(http_req->method_n == E_FAIL)
          {
            return E_FAIL;
          }
          http_req->method_end = p;
          parser_state = RL_SP_AFTER_METHOD;
          break;
        }
        break;

      case RL_SP_AFTER_METHOD:
        if (ch == '/') {
          /*assign start and end of schema,host,port and start of uri,path,pathname*/
          http_req->schema_start = p;
          http_req->schema_end = p;
          http_req->host_start = p;
          http_req->host_end = p;
          http_req->port_start = p;
          http_req->port_end = p;
          /*next state is RL_PATH*/
          http_req->uri_start = p;
          http_req->path_start = p;
          http_req->pathname_start = p;
          parser_state = RL_PATH;
        } else {
          char c = ch | 0x20; //convert to lower case
          /*if lower case alphabets, assign start of schema and uri, next state is RL_SCHEMA*/
          if(c >= 'a' && c<='z')
          {
            http_req->schema_start = p;
            http_req->uri_start = p;
            parser_state = RL_SCHEMA;
          }
          /*if not space(''), fails*/
          else if(c!=' ')
          {
            return E_FAIL;
          }
        }
        break;

      case RL_SCHEMA:
        /*on lower case alphabets break ie schema can have lower case alphabets*/
        if(ch >= 'a' && ch<='z')
        {
          break;
        }
        /*schema ends on ':' ,next state is RL_SCHEMA_SLASH*/
        else if(ch ==':')
        {
          parser_state = RL_SCHEMA_SLASH;
        }
        /*schema ends on ':' ,next state is RL_SCHEMA_SLASH*/
        /*fails on all other inputs*/
        else 
        {
          return E_FAIL;
        }
        break;

      case RL_SCHEMA_SLASH:
        /*on '/' assign next state, fails on all other inputs*/
        if(ch =='/')
        {
          parser_state = RL_SCHEMA_SLASH_SLASH;
        }
        else {
          return E_FAIL;
        }
        break;

      case RL_SCHEMA_SLASH_SLASH:
        /*on '/' - assign next state, for start of host assign next position, fails on all other inputs*/
        if(ch == '/')
        {
          parser_state = RL_HOST;
          http_req->host_start = p;
        }
        else {
          return E_FAIL;
        }
        break;

      case RL_HOST:
        if(islower(ch) || isdigit(ch) || ch == '-' || ch == '.')
        {
          break;
        }
        else if(ch == ':')
        {
          http_req->host_end = p;
          http_req->port_start = p;
          parser_state = RL_PORT;
        }
        else if(ch == '/')
        {
          http_req->host_end = p;
          http_req->port_end = p;
          http_req->path_start = p;
          http_req->pathname_start = p;
          parser_state = RL_PATH;
        }
        else if (ch ==' ') {
          http_req->host_end = p;
          http_req->uri_end = p;
          http_req->port_start = p;
          http_req->port_end = p;
          http_req->path_end = p;
          http_req->pathname_end = p;
          parser_state = RL_VERSION_START;
        }
        else {
          return E_FAIL;
        }
        /*host can have lower case alphabets, numbers, '-', '.' */
        /*on ':' - host ends, for start of port assign next position, next state is RL_PORT*/
        /*on '/' - host ends, assign start of path,pathname, end of port, next state is RL_PATH*/
        /*on ' ' - host ends, assign end of uri, start and end of port,path,pathname, next state is RL_VERSION_START*/
        /*on all other input, fails*/
        break;

      case RL_PORT:
        /*port can only have numbers */
        if(isdigit(ch))
        {
          break;
        }
        else if(ch =='/')
        {
          http_req->port_end = p;
          http_req->path_start = p;
          http_req->pathname_start = p;
          parser_state = RL_PATH;
        }
        else if(ch == ' ')
        {
          http_req->port_end = p;
          http_req->uri_end = p;
          http_req->path_start = p;
          http_req->path_end = p;
          http_req->pathname_start = p;
          http_req->pathname_end = p;
          parser_state = RL_VERSION_START;
        }
        /*on '/' - port ends, assign start of path,pathname, next state is RL_PATH*/
        /*on ' ' - port ends, assign end of uri, start and end of path,pathname, next state is RL_VERSION_START*/
        /*on all other input, fails*/
        else {
          return E_FAIL;
        }
        break;

      case RL_PATH:
        if(ch == ' ')
        {
          http_req->path_end = p;
          http_req->pathname_end = p;
          http_req->uri_end = p;
          parser_state = RL_VERSION_START;
        }
        else if(ch =='?' || ch == '&' || ch == '=' || ch == '#')
        {
          http_req->path_end = p;
          parser_state = RL_PATHNAME;
        }
        else if(ch == CR || ch == LF)
        {
          return E_FAIL;
        }
        /*on ' ' - path ends, assign end of path,pathname,uri next state is RL_VERSION_START*/
        /*on '?'or'&'or'='or'#' - assign end of path, next state is RL_PATHNAME*/
        /*on CR or LF, fails*/
        break;

      case RL_PATHNAME:
        if(ch == ' ')
        {
          http_req->uri_end = p;
          http_req->pathname_end = p;
          parser_state = RL_VERSION_START;
        }
        else if(ch == CR || ch == LF)
        {
          return E_FAIL;
        }
        /*on ' ' - assign end of uri,pathname, next state is RL_VERSION_START*/
        /*on CR or LF, fails*/
        break;

      case RL_VERSION_START:
        /*can have space*/
        if(ch ==' ')
        {
          break;
        }
        if(ch == 'H')
        {
          parser_state = RL_VERSION_H;
        }
        else {
          return E_FAIL;
        }
        /*on 'H' - next state is RL_VERSION_H*/
        /*fails on all other input*/
        break;

      case RL_VERSION_H:
        /*fill this*/
        if(ch == 'T')
        {
          parser_state = RL_VERSION_HT;
        }
        else {
          return E_FAIL;
        }
        break;

      case RL_VERSION_HT:
        /*fill this*/
        if(ch == 'T')
        {
          parser_state = RL_VERSION_HTT;
        }
        else {
          return E_FAIL;
        }
        break;

      case RL_VERSION_HTT:
        /*fill this*/
        if(ch == 'P')
        {
          parser_state = RL_VERSION_HTTP;
        }
        else {
          return E_FAIL;
        }
        break;

      case RL_VERSION_HTTP:
        /*fill this*/
        if(ch == '/')
        {
          parser_state = RL_VERSION_HTTP_SLASH;
        }
        else {
          return E_FAIL;
        }
        break;

      case RL_VERSION_HTTP_SLASH:
        /*on '1' - assign major, next state is RL_VERSION_MAJOR, fails on all other inputs*/
        if(ch == '1')
        {
          http_req->http_major = p;
          parser_state = RL_VERSION_MAJOR;
        }
        else {
          return E_FAIL;
        }
        break;

      case RL_VERSION_MAJOR:
        /*fill ths*/
        if(ch == '.')
        {
          parser_state = RL_VERSION_DOT;
        }
        else {
          return E_FAIL;
        }
        break;

      case RL_VERSION_DOT:
        /*on '0' or '1' - assign minor to next position, next state is RL_VERSION_MINOR, fails on other inputs*/
        if(ch == '1' || ch == '0')
        {
          http_req->http_minor = p;
          parser_state = RL_VERSION_MINOR;
        }
        else {
          return E_FAIL;
        }
        break;

      case RL_VERSION_MINOR:
        if (ch == CR) {
          parser_state = RL_CR;
        } else if (ch == LF) {
          parser_state = RL_LF;
        } else {
          return E_FAIL;
        }
        break;

      case RL_CR:
        /*fill this*/
        if (ch == LF) {
          parser_state = RL_LF;
        } 
        else {
          return E_FAIL;
        }
        break;

      case RL_LF:
        if (http_req->request_line_end == NULL) {
          http_req->request_line_end = p;
        }
        http_req->parser_state = H_START;
        buff->pos = p;
        return OK;

      default:
        return E_FAIL;
    }
  }

  return E_AGAIN;
}

int xps_http_parse_header_line(xps_http_req_t *http_req, xps_buffer_t *buff) {
  assert(http_req != NULL);
  assert(buff != NULL);

  u_char *p_ch = buff->pos;
  xps_http_parser_state_t parser_state = http_req->parser_state;

  for (u_char *p = p_ch; *(p)!='\0'; p++) {
    char ch = *p;

    switch (parser_state) {
      case H_START:
        /*fill this*/
        if(isalpha(ch))
        {
          http_req->header_key_start = p;
          parser_state = H_NAME;
          break;
        }
        else
        {
          return E_FAIL;
        }
        break;

      case H_NAME:
        /*fill this*/
        if(ch == ':')
        {
          parser_state = H_COLON;
          http_req->header_key_end = p;
          break;
        }
        else if(ch == '-')
        {
          break;
        }
        else if(isalpha(ch))
        {
          break;
        }
        else {
          return E_FAIL;
        }
        break;

      case H_COLON:
        /*fill this*/
        if(ch == ' ')
        {
          parser_state = H_SP_AFTER_COLON;
          break;
        }
        else if(ch == '\n')
        {
          break;
        }
        else {
          parser_state = H_VAL;
          break;
        }
        break;

      case H_SP_AFTER_COLON:
        /*fill this*/
        if(ch == ' ')
        {
          break;
        }
        else {
          parser_state = H_VAL;
          http_req->header_val_start = p;
          break;
        }
        break;

      case H_VAL:
        if (ch == CR || ch == LF) {
          /*fill this*/
          parser_state = (ch == CR) ? H_CR : H_LF;
          http_req->header_val_end = p;
        }
        break;

      case H_CR:
        /*fill this*/
        if(ch == LF)
        {
          parser_state = H_LF;
          break;
        }
        else {
          return E_FAIL;
        }
        break;

      case H_LF:
        if (ch == LF) {
          /*fill this*/
          parser_state = H_LF_LF;
        } else if (ch == CR) {
          /*fill this*/
          parser_state = H_LF_CR;
        } else {
          /*fill this*/
          parser_state = H_START;
          buff->pos = p;
          return E_NEXT; // This header is done, repeat for the next
        }
        break;

      case H_LF_LF:
        buff->pos = p;
        http_req->parser_state = H_START;
        return OK; // HTTP complete header section done

      case H_LF_CR:
        if (ch == LF) {
          buff->pos = p;
          http_req->parser_state = H_START;
          return OK; // HTTP complete header section done
        } else {
          return E_FAIL;
        }
        break;

      default:
        return E_FAIL;
    }
  }

  return E_AGAIN;
}

const char *xps_http_get_header(vec_void_t *headers, const char *key) {
  /*Validate params*/
  assert(headers!=NULL);
  assert(key!=NULL);
  for (int i = 0; i < headers->length; i++) {
    xps_keyval_t *header = headers->data[i];/*fill this*/
    if (strcasecmp(header->key, key) == 0)
      /*fill this*/return header->val;

  }
	return NULL;
}

xps_buffer_t *xps_http_serialize_headers(vec_void_t *headers) {
  /*assert*/
  assert(headers != NULL);
	/*create a buffer and initialize first byte to null terminator*/
  xps_buffer_t * buff = xps_buffer_create(DEFAULT_BUFFER_SIZE, 0, NULL);
  buff->data[0] = '\0';
  for (int i = 0; i < headers->length; i++) {
    /*get required length to store a header*/
    xps_keyval_t *header = headers->data[i];
    int header_str_len = strlen(header->key) + 2 + strlen(header->val) + 2;
    char header_str[header_str_len];
    sprintf(header_str, "%s: %s\r\n", header->key, header->val);
    if ((buff->size - buff->len) < header_str_len) { //buffer is small
      u_char *new_data = realloc(buff->data, 2*buff->size);/*realloc() buffer to twice size*/
      if(new_data == NULL)
      {
        logger(LOG_ERROR, "xps_http_serialize_headers", "realloc failed");
        return NULL;
      }
      /*handle error*/
      /*update buff->data and buff->size*/
      buff->data = new_data;
      buff->size = 2*buff->size;
    }
    strcat(buff->data, header_str);
    buff->len = strlen(buff->data);
  }
	return buff;
}

int xps_http_set_header(vec_void_t *headers, const char *key, const char *val) {
  /* Validate params */
  assert(headers!=NULL);
  assert(key!=NULL);
  assert(val!=NULL);
  xps_keyval_t *header = (xps_keyval_t *)malloc(sizeof(xps_keyval_t))/* fill this */;
  if (header == NULL) {
    logger(LOG_ERROR, "xps_http_set_header()", "malloc() failed for 'header'");
    return E_FAIL;
  }

  /* allocate memory for header->key and header->val */
  header->key = (char *)malloc(strlen(key)+1);
  header->val = (char *)malloc(strlen(val)+1);

  if (header->key == NULL || header->val == NULL) {
    free(header->key);
    free(header->val);
    free(header);
    logger(LOG_ERROR, "xps_http_set_header()",
           "malloc() failed for 'header->key' or 'header->val'");
    return E_FAIL;
  }

  /* copy contents of key and value given into the header fields */
  strcpy(header->key, key);
  strcpy(header->val, val);

  /* add the header to header list */
  vec_push(headers, header);

  return OK;
}