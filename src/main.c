#include "xps.h"

// Global variables
xps_core_t *core;

void sigint_handler(int signum);

int main() {
  signal(SIGINT, sigint_handler);
  
  core = xps_core_create();

  /* 'start' core instance */
  xps_core_start(core);
}

void sigint_handler(int signum)
{
  logger(LOG_WARNING, "sigint_handler()", "SIGINT recieved");

  xps_core_destroy(core);

  exit(EXIT_SUCCESS);
}

// docker compose -f docker-compose.dev.yaml down
// docker compose -f docker-compose.dev.yaml up -d
// docker compose -f docker-compose.dev.yaml restart
// docker compose -f docker-compose.dev.yaml logs -f
// cat /home/lesmithbro/Documents/My-Stuff/Downloads/QuartusLiteSetup-20.1.1.720-linux.run | ./sender