#include "list_window.h"
#include "data.h"
#include "comm.h"
#include "article_window.h"

static Window *s_window;
static MenuLayer *s_menu;
static char s_status[64] = "Locating...";
static char s_header[36] = "Nearby Wiki";

static int16_t prv_get_header_height(MenuLayer *menu, uint16_t section,
                                     void *context) {
  return 16;
}

static void prv_draw_header(GContext *ctx, const Layer *cell_layer,
                            uint16_t section, void *context) {
  GRect bounds = layer_get_bounds(cell_layer);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  graphics_context_set_text_color(ctx, GColorBlack);

  // Left: fetch status ("Loc update at 12:04")
  graphics_draw_text(ctx, s_header, font,
                     GRect(2, -3, bounds.size.w - 56, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);

  // Right: clock glyph + current time
  static char s_now[8];
  time_t now = time(NULL);
  strftime(s_now, sizeof(s_now), clock_is_24h_style() ? "%H:%M" : "%I:%M",
           localtime(&now));
  GPoint center = GPoint(bounds.size.w - 48, 8);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, 5);
  graphics_draw_line(ctx, center, GPoint(center.x, center.y - 3));
  graphics_draw_line(ctx, center, GPoint(center.x + 2, center.y));
  graphics_draw_text(ctx, s_now, font,
                     GRect(bounds.size.w - 40, -3, 38, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
                     NULL);
}

static void prv_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}

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

void list_window_set_header(const char *header) {
  snprintf(s_header, sizeof(s_header), "%s", header);
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}

void list_window_show_fetch_time(const char *prefix,
                                 const char *short_prefix) {
  struct tm *lt = localtime(&g_list_fetch_time);
  char tbuf[8];
  strftime(tbuf, sizeof(tbuf), clock_is_24h_style() ? "%H:%M" : "%I:%M", lt);
  if (s_window &&
      layer_get_bounds(window_get_root_layer(s_window)).size.w < 180) {
    prefix = short_prefix;
  }
  char header[36];
  snprintf(header, sizeof(header), "%s %s", prefix, tbuf);
  list_window_set_header(header);
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
      .get_header_height = prv_get_header_height,
      .draw_header = prv_draw_header,
      .draw_row = prv_draw_row,
      .select_click = prv_select_click,
      .select_long_click = prv_select_long_click,
  });
  // B&W: dithered cerulean makes white row text hard to read; use black
  menu_layer_set_highlight_colors(
      s_menu, PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack),
      GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick);
}

static void prv_window_unload(Window *window) {
  tick_timer_service_unsubscribe();
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
