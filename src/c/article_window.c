#include "article_window.h"
#include "data.h"
#include "comm.h"

#define MARGIN 4

static Window *s_window;
static ScrollLayer *s_scroll;
static TextLayer *s_title_layer;
static TextLayer *s_body_layer;
static int s_index = -1;

static void prv_layout(void) {
  GRect bounds = layer_get_bounds(window_get_root_layer(s_window));
  int width = bounds.size.w - 2 * MARGIN;

  // Content size measurement is clipped to the layer's frame, so grow the
  // frame before measuring, then shrink it to fit.
  layer_set_frame(text_layer_get_layer(s_title_layer),
                  GRect(MARGIN, 0, width, 500));
  GSize title_size = text_layer_get_content_size(s_title_layer);
  layer_set_frame(text_layer_get_layer(s_title_layer),
                  GRect(MARGIN, 0, width, title_size.h + 8));

  layer_set_frame(text_layer_get_layer(s_body_layer),
                  GRect(MARGIN, title_size.h + 8, width, 4000));
  GSize body_size = text_layer_get_content_size(s_body_layer);
  layer_set_frame(text_layer_get_layer(s_body_layer),
                  GRect(MARGIN, title_size.h + 8, width, body_size.h + 12));

  scroll_layer_set_content_size(
      s_scroll, GSize(bounds.size.w, title_size.h + 8 + body_size.h + 16));
}

void article_window_on_summary(bool done) {
  if (!s_window || !window_stack_contains_window(s_window)) {
    return;
  }
  text_layer_set_text(s_body_layer, g_summary);
  prv_layout();
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int width = bounds.size.w - 2 * MARGIN;

  s_scroll = scroll_layer_create(bounds);
  scroll_layer_set_shadow_hidden(s_scroll, true);
  scroll_layer_set_click_config_onto_window(s_scroll, window);

  s_title_layer = text_layer_create(GRect(MARGIN, 0, width, 60));
  text_layer_set_font(s_title_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_title_layer, g_articles[s_index].title);
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_title_layer));

  s_body_layer = text_layer_create(GRect(MARGIN, 60, width, 2000));
  text_layer_set_font(s_body_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_body_layer, "Loading...");
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_body_layer));

  layer_add_child(root, scroll_layer_get_layer(s_scroll));
  prv_layout();
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_body_layer);
  scroll_layer_destroy(s_scroll);
  window_destroy(s_window);
  s_window = NULL;
}

void article_window_push(int article_index) {
  s_index = article_index;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_window_load,
      .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
  comm_request_summary(article_index);
}
