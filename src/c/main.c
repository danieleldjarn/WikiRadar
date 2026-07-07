#include <pebble.h>
#include "cache.h"
#include "comm.h"
#include "data.h"
#include "list_window.h"

static char s_glance_text[80];

static void prv_glance_reload(AppGlanceReloadSession *session, size_t limit,
                              void *context) {
  if (limit < 1) {
    return;
  }
  const AppGlanceSlice slice = {
      .layout = {
          .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
          .subtitle_template_string = s_glance_text,
      },
      .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  app_glance_add_slice(session, slice);
}

// Launcher subtitle after exit: the nearest article and its distance
static void prv_update_glance(void) {
  if (g_article_count == 0) {
    return;
  }
  char dist[16];
  data_format_distance(data_distance_to(&g_articles[0]), dist, sizeof(dist));
  snprintf(s_glance_text, sizeof(s_glance_text), "%s · %s",
           g_articles[0].title, dist);
  app_glance_reload(prv_glance_reload, NULL);
}

int main(void) {
  comm_init();
  data_load_units();
  data_load_read_set();
  bool cached = cache_load_list();
  list_window_push();
  if (cached) {
    list_window_show_fetch_time("Loc as of", "As of");
  }
  comm_request_list();
  app_event_loop();
  prv_update_glance();
}
