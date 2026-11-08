#include "config/xps_config.h"
#include "core/xps_core.h"
#include "core/xps_loop.h"
#include "network/xps_listener.h"
#include "utils/xps_logger.h"
#include "utils/xps_utils.h"
#include "xps.h"
#include <unistd.h>

// Global variables
xps_core_t *core;

void sigint_handler(int signum);

int core_create(xps_config_t *config) {
  /*assert*/
  assert(config!=NULL);
  core = xps_core_create(config);
  /*Create listeners*/
  for(int i=0; i<config->_all_listeners.length; i++)
  {
    xps_config_listener_t * conf_listener = config->_all_listeners.data[i];
    xps_listener_t *listener = xps_listener_create(conf_listener->host, conf_listener->port);

    int dup1 = dup(listener->sock_fd);
    xps_listener_t * dup_listener = (xps_listener_t *)malloc(sizeof(xps_listener_t));
    dup_listener->sock_fd = dup1;
    dup_listener->host = str_create(listener->host);
    dup_listener->port = listener->port;
    dup_listener->core = core;
    xps_loop_attach(core->loop,dup1, EPOLLIN | EPOLLET, dup_listener, listener_connection_handler, NULL, NULL);
    vec_push(&core->listeners, dup_listener);

    xps_listener_destroy(listener);
  }

  return 0;
  /*Duplicate (use dup(fd) to duplicate file descriptor) and add listeners to cores*/
  /*Initialize dup_listener values*/
  /*Attach dup_listener to loop*/
  /*Add listener to 'listeners' list of core*/
  /*Destory listeners*/
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigint_handler);
  xps_cliargs_t * cliargs = xps_cliargs_create(argc, argv);
  
  xps_config_t * config = xps_config_create(cliargs->config_path);
  if(core_create(config) == 0)
  {
    logger(LOG_INFO, "main()", "Core created");
  }

  /* 'start' core instance */
  xps_core_start(core);
}


void core_destroy(xps_core_t *core) {
  xps_core_destroy(core);
}

void sigint_handler(int signum)
{
  logger(LOG_WARNING, "sigint_handler()", "SIGINT recieved");

  core_destroy(core);

  exit(EXIT_SUCCESS);
}

// docker compose -f docker-compose.dev.yaml down
// docker compose -f docker-compose.dev.yaml up -d
// docker compose -f docker-compose.dev.yaml restart
// docker compose -f docker-compose.dev.yaml logs -f
// cat /home/lesmithbro/Documents/My-Stuff/Downloads/QuartusLiteSetup-20.1.1.720-linux.run | ./sender