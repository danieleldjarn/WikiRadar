#include "list_window.h"
#include "data.h"
#include "comm.h"
#include "article_window.h"

static Window *s_window;
static MenuLayer *s_menu;
static char s_status[64] = "Locating...";

static uint16_t prv_get_num_rows(MenuLayer *menu, uint16_t section,
                                 void *context) {
  return g_article_count > 0 ? g_article_count : 1;
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer,
                         MenuIndex *cell_index, void *context) {
  if (g_article_count == 0) {
    menu_cell_basic_draw(ctx, cell_layer, s_status, NULL, NULL);
    return;
  }
  Article *a = &g_articles[cell_index->row];
  char dist[16];
  data_format_distance(a->distance_m, dist, sizeof(dist));
  menu_cell_basic_draw(ctx, cell_layer, a->title, dist, NULL);
}

static void prv_select_click(MenuLayer *menu, MenuIndex *cell_index,
                             void *context) {
  if (g_article_count == 0) {
    return;
  }
  article_window_push(cell_index->row);
}

static void prv_select_long_click(MenuLayer *menu, MenuIndex *cell_index,
                                  void *context) {
  g_article_count = 0;
  list_window_set_status("Refreshing...");
  comm_request_list();
}

void list_window_set_status(const char *status) {
  snprintf(s_status, sizeof(s_status), "%s", status);
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}

void list_window_on_list_updated(bool done) {
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
      .get_num_rows = prv_get_num_rows,
      .draw_row = prv_draw_row,
      .select_click = prv_select_click,
      .select_long_click = prv_select_long_click,
  });
  menu_layer_set_highlight_colors(s_menu, GColorVividCerulean, GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
}

void list_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
        .load = prv_window_load,
        .unload = prv_window_unload,
    });
  }
  window_stack_push(s_window, true);
}
