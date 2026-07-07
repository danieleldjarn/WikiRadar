#include <pebble.h>
#include "cache.h"
#include "comm.h"
#include "list_window.h"

int main(void) {
  comm_init();
  bool cached = cache_load_list();
  list_window_push();
  if (cached) {
    list_window_show_fetch_time("As of");
  }
  comm_request_list();
  app_event_loop();
}
