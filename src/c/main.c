#include <pebble.h>
#include "comm.h"
#include "list_window.h"

int main(void) {
  comm_init();
  list_window_push();
  comm_request_list();
  app_event_loop();
}
