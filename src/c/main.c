#include <pebble.h>
#include "cache.h"
#include "comm.h"
#include "data.h"
#include "list_window.h"

int main(void) {
  comm_init();
  data_load_units();
  bool cached = cache_load_list();
  list_window_push();
  if (cached) {
    list_window_show_fetch_time("Loc as of");
  }
  comm_request_list();
  app_event_loop();
}
