#include "xps_config.h"
#include <string.h>

static const char *default_gzip_mimes[] = {
    "text/html",
    "text/css",
    "text/plain",
    "text/javascript",
    "application/javascript",
    "application/json",
    "application/xml",
    "image/svg+xml"
};
static const int num_default_gzip_mimes = sizeof(default_gzip_mimes) / sizeof(default_gzip_mimes[0]);

xps_config_t *xps_config_create(const char *config_path) {
  /*assert*/
  assert(config_path!=NULL);
  /*allocate mem for config*/
  xps_config_t *config = (xps_config_t*)malloc(sizeof(xps_config_t));
  /*get config_json using json_parse_file*/
  JSON_Value* config_json = json_parse_file(config_path);
  config->_config_json = config_json;
  /*initialize fields of config object*/
  JSON_Object *root_object = json_value_get_object(config_json);
  config->config_path = config_path;
  vec_init(&(config->servers));
  vec_init(&(config->_all_listeners));
  /*initialize server_name,servers fields - hint: use json_object_get_string
  ,json_object_get_number,json_object_get_array*/
  JSON_Value * server_name = json_object_get_value(root_object, "server_name");
  JSON_Array * servers = json_object_get_array(root_object, "servers");
  config->server_name = json_value_get_string(server_name);
  for (size_t i = 0; i < json_array_get_count(servers); i++) {
    JSON_Object * serv/*fill this*/ = json_array_get_object(servers, i);
    xps_config_server_t * config_server/*fill this*/ = malloc(sizeof(xps_config_server_t));
    /*initialize and parse the server and push to servers list of config*/
    vec_init(&(config_server->listeners));
    vec_init(&(config_server->hostnames));
    vec_init(&(config_server->routes));
    JSON_Array * listeners = json_object_get_array(serv, "listeners");
    JSON_Array * routes = json_object_get_array(serv, "routes");
    for (size_t j = 0; j < json_array_get_count(listeners); j++)
    {

        JSON_Object *listener = json_array_get_object(listeners, j);
        xps_config_listener_t * config_listener  = (xps_config_listener_t * )malloc(sizeof(xps_config_listener_t));
        config_listener->host = json_object_get_string(listener, "host");
        config_listener->port = json_object_get_number(listener, "port");
        vec_push(&(config_server->listeners), config_listener);
    }
    for (size_t j = 0; j < json_array_get_count(routes); j++)
    {

        JSON_Object *route = json_array_get_object(routes, j);
        xps_config_route_t * config_route  = (xps_config_route_t * )malloc(sizeof(xps_config_route_t));
        config_route->req_path = json_object_get_string(route, "req_path");
        config_route->type = json_object_get_string(route, "type");
        if(strcmp(config_route->type, "file_serve") == 0)
        {
            config_route->dir_path = json_object_get_string(route, "dir_path");
            JSON_Array * indexes = json_object_get_array(route, "index");
            vec_init(&(config_route->index));
            for (size_t k = 0; k < json_array_get_count(indexes); k++)
            {
                char * index  = json_array_get_string(indexes, k);
                vec_push(&(config_route->index), index);
            }
        }
        else if(strcmp(config_route->type, "redirect") == 0)
        {
            config_route->http_status_code = json_object_get_number(route, "http_status_code");
            config_route->redirect_url = json_object_get_string(route, "redirect_url");
        }
        else if(strcmp(config_route->type, "reverse_proxy") == 0)
        {
            JSON_Array * upstreams = json_object_get_array(route, "upstreams");
            vec_init(&(config_route->upstreams));
            for (size_t k = 0; k < json_array_get_count(upstreams); k++)
            {
                char * upstream = json_array_get_string(upstreams, k);
                vec_push(&(config_route->upstreams), upstream);
            }
        }
        JSON_Array *ip_whitelist = json_object_get_array(route, "ip_whitelist");
        JSON_Array *ip_blacklist = json_object_get_array(route, "ip_blacklist");

        if(ip_whitelist && !ip_blacklist){
          for(size_t k = 0; k < json_array_get_count(ip_whitelist)/*iterate through ip_whitelist array*/; k++)
          {
            char * ip = json_array_get_string(ip_whitelist, k);
            vec_push(&(config_route->ip_whitelist), ip/*get ith IP from ip_whitelist array*/);
          }
        }else if(ip_blacklist && !ip_whitelist){
            /*fill this*/
            for(size_t k = 0; k < json_array_get_count(ip_blacklist); k++)
            {
              char * ip = json_array_get_string(ip_blacklist, k);
              vec_push(&(config_route->ip_blacklist), ip);
            }
        }else if(ip_whitelist && ip_blacklist) {
            /*fill this*/
            for(size_t k = 0; k < json_array_get_count(ip_whitelist); k++)
            {
              int flag = 0;
              char * ipw = json_array_get_string(ip_whitelist, k);
              for(size_t l = 0; l < json_array_get_count(ip_blacklist); l++)
              {
                char * ipb = json_array_get_string(ip_blacklist, l);
                if(strcmp(ipw, ipb) == 0)
                {
                  flag = 1;
                  break;
                }
              }
              if(flag == 0)
              {
                vec_push(&(config_route->ip_whitelist), ipw);
              }
            }
        }
        config_route->gzip_enable = json_object_get_boolean(route, "gzip_enable");
        config_route->gzip_level = json_object_get_number(route, "gzip_level");
        vec_init(&(config_route->gzip_mime_types));
        JSON_Array * gzip_mime_types = json_object_get_array(route, "gzip_mime_types");
        for (size_t k = 0; k < json_array_get_count(gzip_mime_types); k++)
        {
            char * gzip_mime_type = json_array_get_string(gzip_mime_types, k);
            vec_push(&(config_route->gzip_mime_types), gzip_mime_type);
        }
        vec_push(&(config_server->routes), config_route);
    }
    vec_push(&(config->servers), config_server);
  }

  /*initialize and set up the _all_listeners array*/
  for(int i=0; i<config->servers.length; i++)
  {
    xps_config_server_t * config_server = config->servers.data[i];
    for(int j=0; j<config_server->listeners.length; j++)
    {
        xps_config_listener_t * config_listener = config_server->listeners.data[j];
        vec_push(&(config->_all_listeners), config_listener);
    }
  }
  return config;
}

void xps_config_destroy(xps_config_t *config)
{
  assert(config!=NULL);

  for(int i=0; i< config->servers.length; i++)
  {
    xps_config_server_t *server = config->servers.data[i];
    for(int j=0; j < server->listeners.length; j++)
    {
      free(server->listeners.data[j]);
    }
    vec_deinit(&(server->listeners));
    vec_deinit(&(server->hostnames));

    for(int j=0; j < server->routes.length; j++)
    {
      xps_config_route_t * route = server->routes.data[j];
      vec_deinit(&(route->index));
      vec_deinit(&(route->upstreams));
      free(route);
    }
    vec_deinit(&(server->routes));
    free(server);
  }

  vec_deinit(&(config->servers));
  vec_deinit(&(config->_all_listeners));
  json_value_free(config->_config_json);
  free(config);
}

xps_config_lookup_t *xps_config_lookup(xps_config_t *config, xps_http_req_t *http_req,
                                       xps_connection_t *client, int *error) {
  /*assert*/
  assert(config!=NULL);
  assert(http_req!=NULL);
  assert(client!=NULL);
 *error = E_FAIL;
  /*get host,accept encoding,pathname from http_req*/
  char * host = http_req->host;
  char * pathname = http_req->pathname;
  // Step 1: Find matching server block
  int target_server_index = -1;
  bool has_matching_listener = false;
  bool has_matching_hostname = false;
  for (int i = 0;i < config->servers.length/*fill this*/; i++) {
    xps_config_server_t *server = config->servers.data[i];/*fill this*/
		// Check if client listener is present in server
    for (int j = 0; j < server->listeners.length/*fill this*/; j++) {
      /*fill this*/
      xps_config_listener_t * list_config = server->listeners.data[j];
      if (list_config->port == client->listener->port && (strcmp(list_config->host,client->listener->host) ==0 )) {
        has_matching_listener = true;
        if(list_config->host == client->listener->host)
            has_matching_hostname = true;
        break;
      }
    }
	if (!has_matching_listener)
      continue;
		/* Check if host header matches any hostname*/
		// NOTE: if not hostnames provided, it is considered a match
  if(server->hostnames.length == 0)
  {
    has_matching_hostname = true;
  }
    if (has_matching_hostname) {
      target_server_index = i;
      break;
    }
    has_matching_listener = false;
  }

  if(target_server_index == -1)
  {
    *error = E_NOTFOUND;
    return NULL;
  }
  xps_config_server_t *server = config->servers.data[target_server_index];

  /*Find matching route block*/
  // Route matching uses prefix matching with longest-match-first strategy.
  // This is important because:
  // - For file serving routes (e.g., "/"), we need to match any path under it
  //   (e.g., "/index.html", "/css/style.css" should all match route "/")
  // - For specific routes (e.g., "/api"), we want them to take precedence over "/"
  //
  // Example: If we have routes "/" and "/api"
  // - Request "/index.html" matches "/" only → serves file from "/"
  // - Request "/api/users" matches both "/" and "/api" → use "/api" (longest match)

  xps_config_route_t *route = NULL;
  size_t best_match_len = 0;  // Track the longest matching route path

  for (int i = 0; i < server->routes.length; i++) {
		xps_config_route_t *current_route = server->routes.data[i];
		size_t route_path_len = strlen(current_route->req_path);/*find current route requested path length*/

		// Check if this route's path is a prefix of the request path
		if (str_starts_with(pathname, current_route->req_path)) {
				// If this is a longer match than we've found so far, use it
			if (route_path_len > best_match_len) {
				/*fill the rest*/
        best_match_len = route_path_len;
        route = current_route;
			}
		}
  }
//133
  if (route == NULL) {
      *error = E_NOTFOUND;  // No matching route found - 404
      return NULL;
  }
  /* Init values of lookup*/
  xps_config_lookup_t * lookup = (xps_config_lookup_t*)malloc(sizeof(xps_config_lookup_t));
  // File serve
  lookup->file_path = NULL;
  lookup->dir_path = NULL;
  lookup->ip_blacklist = route->ip_blacklist;
  lookup->ip_whitelist = route->ip_whitelist;
  lookup->gzip_enable = route->gzip_enable;
  lookup->gzip_level = route->gzip_level;
  lookup->gzip_mime_eligible = false;
  if(strcmp("redirect",route->type) == 0)
  {
    lookup->type = REQ_REDIRECT;
    lookup->http_status_code = route->http_status_code;
    lookup->redirect_url = route->redirect_url;
    *error = OK;
    return lookup;
  }
  else if(strcmp("file_serve",route->type) == 0)
  {
    lookup->type = REQ_FILE_SERVE;
  }
  else if(strcmp("reverse_proxy",route->type) == 0)
  {
    lookup->type =  REQ_REVERSE_PROXY;
    if(route->upstreams.length < 0)
    {
      logger(LOG_DEBUG, "xps_config_lookup()", "no upstream server in the json file");
    }
    char * ups = route->upstreams.data[0];
    lookup->upstream = ups;
    *error = OK;
    return lookup;
  }
  if (lookup->type == REQ_FILE_SERVE) {
    const char * relative_path = pathname + strlen(route->req_path);
    if(relative_path[0] == '/')
    {
      relative_path++;
    }
    const char *base_dir = route->dir_path;
    if(route->dir_path == NULL)
    {
      base_dir = "../public";
    }
    char *resource_path = path_join(base_dir ,relative_path);/*use path_join to join paths*/
    if (!is_abs_path(resource_path)) {
      /* we require abosulte path so we need to see
        if the current path is absolute or not */
      /*fill here*/
      char * abs_path = realpath(resource_path, NULL);
      if(abs_path)
      {
        free(resource_path);
        resource_path = abs_path;
      }
      else {
        logger(LOG_DEBUG, "xps_config_lookup()", "abs path returned null");
      }
    }
    // is file
    if (is_file(resource_path)) {
      lookup->file_path = resource_path;/*fill here*/;
      

    } else if (is_dir(resource_path)) { // is directory
      /* If request is for a directory, serve the index file (e.g. index.html)
       * instead of showing the directory listing. */
      bool index_file_found = false;
      for (int i = 0; i < route->index.length/*fill here*/; i++) {
        char * index = route->index.data[i];
        char *index_file = path_join(resource_path ,index/*fill here*/);
       /*fill here*/
       if(is_file(index_file))
       {
        lookup->file_path = index_file;
        index_file_found = true;
        free(resource_path);
        break;
       }
       free(index_file);
      }
      if (!index_file_found) {
        /*no index file so free the resource_path*/
        lookup->dir_path = resource_path;
      }
    } else {
      /*no matching type so free resource_path*/
      free(resource_path);
    }
    *error = OK;
    if(lookup->file_path !=NULL)
    {
      const char *mime_type = xps_get_mime(lookup->file_path);
      if(mime_type!=NULL)
      {
        for(int p=0; p< route->gzip_mime_types.length; p++)
        {
          if(strcmp(route->gzip_mime_types.data[p], mime_type) == 0)
          {
            lookup->gzip_mime_eligible = true;
            break;
          }
        }
        if(lookup->gzip_mime_eligible == false)
        {
          for(int p=0; p< num_default_gzip_mimes; p++)
          {
            if(strcmp(default_gzip_mimes[p], xps_get_mime(lookup->file_path))==0)
            {
              lookup->gzip_mime_eligible = true;
              break;
            }
          }
        }
      }
    }
    return lookup;
  }
  return NULL;
}

void xps_config_lookup_destroy(xps_config_lookup_t * config_lookup,  xps_core_t *core)
{
  if(config_lookup == NULL)
  {
    return;
  }
  free(config_lookup->file_path);
  free(config_lookup->dir_path);
  free(config_lookup);
}