#include "article_window.h"
#include "data.h"
#include "comm.h"
#include "cache.h"
#include "compass_window.h"

#define MARGIN 4
#define DIST_ROW_H 22
#define GLYPH_W 16
#define COORD_ROW_H 18
// Below this width the coordinates get their own row under the distance
#define NARROW_W 180

static Window *s_window;
static ScrollLayer *s_scroll;
static TextLayer *s_title_layer;
static TextLayer *s_body_layer;
static TextLayer *s_dist_layer;
static TextLayer *s_coord_layer;
static Layer *s_glyph_layer;
static char s_dist_text[24];
static char s_coord_text[32];
static int s_index = -1;
// Copy of the article: the list can be replaced by auto-refresh mid-read
static Article s_article;
static bool s_have_cached_summary;

// Format degrees * 1e5 as "-21.9426" (4 decimals, ~11 m precision)
static void prv_format_coord(int32_t v_1e5, char *buf, size_t buf_len) {
  int32_t r = (v_1e5 + (v_1e5 >= 0 ? 5 : -5)) / 10;
  int32_t whole = r / 10000;
  int32_t frac = r % 10000;
  if (frac < 0) {
    frac = -frac;
  }
  snprintf(buf, buf_len, "%s%d.%04d", (r < 0 && whole == 0) ? "-" : "", (int)whole,
           (int)frac);
}

// Small compass-needle glyph pointing up-right, hinting at the compass view
static void prv_glyph_update(Layer *layer, GContext *ctx) {
  GPathInfo info = {
      .num_points = 4,
      .points = (GPoint[]){{13, 2}, {7, 15}, {6, 9}, {0, 8}},
  };
  GPath *path = gpath_create(&info);
  graphics_context_set_fill_color(ctx, GColorVividCerulean);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void prv_update_distance(void) {
  data_format_distance(data_distance_to(&s_article), s_dist_text,
                       sizeof(s_dist_text));
  text_layer_set_text(s_dist_layer, s_dist_text);
}

void article_window_on_location(void) {
  if (!s_window || !window_stack_contains_window(s_window)) {
    return;
  }
  prv_update_distance();
}

// Measure text independently of layer render state;
// text_layer_get_content_size is only reliable after a render.
static int16_t prv_text_height(const char *text, const char *font_key,
                               int width) {
  GSize size = graphics_text_layout_get_content_size(
      text, fonts_get_system_font(font_key), GRect(0, 0, width, 8000),
      GTextOverflowModeWordWrap, GTextAlignmentLeft);
  return size.h;
}

static void prv_layout(void) {
  GRect bounds = layer_get_bounds(window_get_root_layer(s_window));
  int width = bounds.size.w - 2 * MARGIN;

  int16_t title_h =
      prv_text_height(s_article.title, FONT_KEY_GOTHIC_24_BOLD, width);
#ifdef PBL_ROUND
  // Flowed text can take more lines than the rectangular measurement
  title_h += title_h / 2;
#endif

  int body_y;
#ifdef PBL_ROUND
  // Page 1 is a "cover" (title + distance + coords, centered both ways);
  // the body starts exactly at the next page boundary so the flow engine
  // breaks pages between lines instead of clipping them at the bezel.
  int cover_h = title_h + 10 + DIST_ROW_H - 2 + COORD_ROW_H;
  int title_y = (bounds.size.h - cover_h) / 2;
  if (title_y < 12) {
    title_y = 12;
  }
  layer_set_frame(text_layer_get_layer(s_title_layer),
                  GRect(MARGIN, title_y, width, title_h + 8));
  int dist_y = title_y + title_h + 10;
  layer_set_frame(text_layer_get_layer(s_dist_layer),
                  GRect(MARGIN, dist_y, width, DIST_ROW_H));
  int coord_y = dist_y + DIST_ROW_H - 2;
  layer_set_frame(text_layer_get_layer(s_coord_layer),
                  GRect(MARGIN, coord_y, width, COORD_ROW_H));
  body_y = bounds.size.h;
#else
  layer_set_frame(text_layer_get_layer(s_title_layer),
                  GRect(MARGIN, 0, width, title_h + 8));

  int dist_y = title_h + 8;
  layer_set_frame(s_glyph_layer, GRect(MARGIN, dist_y + 2, GLYPH_W, 18));
  layer_set_frame(text_layer_get_layer(s_dist_layer),
                  GRect(MARGIN + GLYPH_W + 2, dist_y, width - GLYPH_W - 2,
                        DIST_ROW_H));
  if (bounds.size.w < NARROW_W) {
    // Not enough room next to the distance; coordinates get their own row
    int coord_y = dist_y + DIST_ROW_H - 2;
    layer_set_frame(text_layer_get_layer(s_coord_layer),
                    GRect(MARGIN + GLYPH_W + 2, coord_y,
                          width - GLYPH_W - 2, COORD_ROW_H));
    body_y = coord_y + COORD_ROW_H + 2;
  } else {
    layer_set_frame(text_layer_get_layer(s_coord_layer),
                    GRect(MARGIN + GLYPH_W + 2, dist_y + 3,
                          width - GLYPH_W - 2, DIST_ROW_H));
    body_y = dist_y + DIST_ROW_H + 2;
  }
#endif
  int16_t body_h = prv_text_height(text_layer_get_text(s_body_layer),
                                   FONT_KEY_GOTHIC_24, width);
#ifdef PBL_ROUND
  body_h += body_h / 2;
#endif
  layer_set_frame(text_layer_get_layer(s_body_layer),
                  GRect(MARGIN, body_y, width, body_h + 12));

  int total_h = body_y + body_h + 16;
#ifdef PBL_ROUND
  // Paged scrolling snaps by screenfuls; round content up to a full page
  total_h = ((total_h + bounds.size.h - 1) / bounds.size.h) * bounds.size.h;
  // The flow/paging origin is captured at enable time, so re-enable after
  // the frames have moved to their final positions
  text_layer_enable_screen_text_flow_and_paging(s_title_layer, 8);
  text_layer_enable_screen_text_flow_and_paging(s_body_layer, 8);
#endif
  scroll_layer_set_content_size(s_scroll, GSize(bounds.size.w, total_h));
}

static void prv_select_handler(ClickRecognizerRef recognizer, void *context) {
  compass_window_push(&s_article);
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_handler);
}

void article_window_on_summary(bool done) {
  if (!s_window || !window_stack_contains_window(s_window)) {
    return;
  }
  text_layer_set_text(s_body_layer, g_summary);
  prv_layout();
  if (done) {
    cache_save_summary(s_article.title);
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int width = bounds.size.w - 2 * MARGIN;

  s_scroll = scroll_layer_create(bounds);
  scroll_layer_set_shadow_hidden(s_scroll, true);
  scroll_layer_set_callbacks(s_scroll, (ScrollLayerCallbacks){
      .click_config_provider = prv_click_config,
  });
  scroll_layer_set_click_config_onto_window(s_scroll, window);

  s_title_layer = text_layer_create(GRect(MARGIN, 0, width, 60));
  text_layer_set_font(s_title_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_title_layer, s_article.title);
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_title_layer));

  s_glyph_layer = layer_create(GRect(MARGIN, 60, GLYPH_W, 18));
  layer_set_update_proc(s_glyph_layer, prv_glyph_update);
  scroll_layer_add_child(s_scroll, s_glyph_layer);

  s_dist_layer = text_layer_create(GRect(MARGIN + GLYPH_W + 2, 60,
                                         width - GLYPH_W - 2, DIST_ROW_H));
  text_layer_set_font(s_dist_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_dist_layer, GColorDarkGray);
  text_layer_set_background_color(s_dist_layer, GColorClear);
  prv_update_distance();
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_dist_layer));

  s_coord_layer = text_layer_create(GRect(MARGIN + GLYPH_W + 2, 60,
                                          width - GLYPH_W - 2, DIST_ROW_H));
  text_layer_set_font(s_coord_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_color(s_coord_layer, GColorDarkGray);
  text_layer_set_background_color(s_coord_layer, GColorClear);
#ifdef PBL_ROUND
  text_layer_set_text_alignment(s_coord_layer, GTextAlignmentCenter);
#else
  text_layer_set_text_alignment(s_coord_layer, bounds.size.w < NARROW_W
                                                   ? GTextAlignmentLeft
                                                   : GTextAlignmentRight);
#endif
  char lat_buf[12], lon_buf[12];
  prv_format_coord(s_article.lat, lat_buf, sizeof(lat_buf));
  prv_format_coord(s_article.lon, lon_buf, sizeof(lon_buf));
  snprintf(s_coord_text, sizeof(s_coord_text), "%s, %s", lat_buf, lon_buf);
  text_layer_set_text(s_coord_layer, s_coord_text);
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_coord_layer));

  s_body_layer = text_layer_create(GRect(MARGIN, 60, width, 2000));
  text_layer_set_font(s_body_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_body_layer,
                      s_have_cached_summary ? g_summary : "Loading...");
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_body_layer));

  layer_add_child(root, scroll_layer_get_layer(s_scroll));

#ifdef PBL_ROUND
  scroll_layer_set_paging(s_scroll, true);
  layer_set_hidden(s_glyph_layer, true);
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_dist_layer, GTextAlignmentCenter);
  text_layer_set_font(s_body_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24));
  // Must be called after the layers are in the view hierarchy
  text_layer_enable_screen_text_flow_and_paging(s_title_layer, 8);
  text_layer_enable_screen_text_flow_and_paging(s_body_layer, 8);
#endif

  prv_layout();
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_body_layer);
  text_layer_destroy(s_dist_layer);
  text_layer_destroy(s_coord_layer);
  layer_destroy(s_glyph_layer);
  scroll_layer_destroy(s_scroll);
  window_destroy(s_window);
  s_window = NULL;
}

void article_window_push(int article_index) {
  s_index = article_index;
  s_article = g_articles[article_index];
  s_have_cached_summary = cache_load_summary(s_article.title);
  data_mark_read(s_article.title);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_window_load,
      .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
  comm_request_summary(article_index);
}
