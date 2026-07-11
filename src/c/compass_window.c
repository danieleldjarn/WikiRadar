#include "compass_window.h"
#include "data.h"

// Round screens: the chord at the top is short, so the title needs two
// flowed lines to fit anything real
#ifdef PBL_ROUND
#define TITLE_H 44
#else
#define TITLE_H 32
#endif
#define DIST_H 36
// The arrow path below is drawn for this dial size; both scale to the
// actual dial radius at window load
#define ARROW_BASE_RADIUS 58

// Radar ping: a ring expands from the center to the dial edge, then
// pauses before the next sweep
#define PING_INTERVAL_MS 4000
#define PING_STEP_MS 40
#define PING_STEP_PX 3

static Window *s_window;
static Layer *s_dial_layer;
static TextLayer *s_title_layer;
static TextLayer *s_dist_layer;
static TextLayer *s_status_layer;
static GPath *s_arrow;
static Article s_article;
static int32_t s_heading = 0;
static bool s_heading_valid = false;
static char s_dist_text[24];
static AppTimer *s_ping_timer;
static int s_ping_radius = -1;  // -1 = between pings

// Arrow pointing up (north) before rotation; sized to leave room for
// the cardinal letters at the rim of an ARROW_BASE_RADIUS dial
#define ARROW_POINTS 7
static const GPoint ARROW_BASE[ARROW_POINTS] = {
    {0, -38}, {19, 3}, {7, 3}, {7, 35}, {-7, 35}, {-7, 3}, {-19, 3}};
static GPoint s_arrow_points[ARROW_POINTS];
static const GPathInfo ARROW_PATH_INFO = {
    .num_points = ARROW_POINTS,
    .points = s_arrow_points,
};

static void prv_update_distance(void) {
  data_format_distance(data_distance_to(&s_article), s_dist_text,
                       sizeof(s_dist_text));
  text_layer_set_text(s_dist_layer, s_dist_text);
}

static void prv_update_status(void) {
  if (!g_has_location) {
    text_layer_set_text(s_status_layer, "Waiting for GPS...");
  } else if (!s_heading_valid) {
    text_layer_set_text(s_status_layer, "Calibrating:\ntilt in a figure 8");
  } else {
    text_layer_set_text(s_status_layer, "");
  }
}

static int prv_dial_radius(void) {
  GRect bounds = layer_get_bounds(s_dial_layer);
  int limit = bounds.size.h < bounds.size.w ? bounds.size.h : bounds.size.w;
  return limit / 2 - 2;
}

static void prv_ping_tick(void *context) {
  if (s_ping_radius < 0) {
    s_ping_radius = 0;  // start a new sweep
  } else {
    s_ping_radius += PING_STEP_PX;
  }
  bool done = s_ping_radius >= prv_dial_radius();
  if (done) {
    s_ping_radius = -1;
  }
  s_ping_timer = app_timer_register(done ? PING_INTERVAL_MS : PING_STEP_MS,
                                    prv_ping_tick, NULL);
  layer_mark_dirty(s_dial_layer);
}

// Compass rose: cardinal letters and minor ticks rotating with the
// heading, so N marks true north on screen
static void prv_draw_rose(GContext *ctx, GPoint center, int radius) {
  static const char *CARDINALS[] = {"N", "E", "S", "W"};
  bool big = radius >= 70;
  GFont font = fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD
                                         : FONT_KEY_GOTHIC_14_BOLD);
  for (int i = 0; i < 12; i++) {
    int32_t angle = (s_heading + i * TRIG_MAX_ANGLE / 12) % TRIG_MAX_ANGLE;
    int32_t sin_v = sin_lookup(angle);
    int32_t cos_v = cos_lookup(angle);
    if (i % 3 == 0) {
      int lr = radius - (big ? 17 : 13);
      GPoint p = GPoint(center.x + sin_v * lr / TRIG_MAX_RATIO,
                        center.y - cos_v * lr / TRIG_MAX_RATIO);
      graphics_context_set_text_color(
          ctx, i == 0 ? PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack)
                      : GColorDarkGray);
      graphics_draw_text(ctx, CARDINALS[i / 3], font,
                         big ? GRect(p.x - 10, p.y - 12, 20, 22)
                             : GRect(p.x - 9, p.y - 10, 18, 18),
                         GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
    } else {
      GPoint p1 = GPoint(center.x + sin_v * (radius - 6) / TRIG_MAX_RATIO,
                         center.y - cos_v * (radius - 6) / TRIG_MAX_RATIO);
      GPoint p2 = GPoint(center.x + sin_v * (radius - 2) / TRIG_MAX_RATIO,
                         center.y - cos_v * (radius - 2) / TRIG_MAX_RATIO);
      graphics_context_set_stroke_color(ctx, GColorDarkGray);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, p1, p2);
    }
  }
}

static void prv_dial_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int radius = prv_dial_radius();

  if (s_ping_radius > 0) {
    graphics_context_set_stroke_color(
        ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorLightGray));
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_circle(ctx, center, s_ping_radius);
  }

  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, radius);
  prv_draw_rose(ctx, center, radius);

  if (!s_heading_valid || !g_has_location) {
    return;
  }

  int32_t bearing = data_bearing_to(&s_article);
  int32_t angle = (s_heading + bearing) % TRIG_MAX_ANGLE;
  gpath_rotate_to(s_arrow, angle);
  gpath_move_to(s_arrow, center);
  graphics_context_set_fill_color(ctx, GColorVividCerulean);
  gpath_draw_filled(ctx, s_arrow);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  gpath_draw_outline(ctx, s_arrow);
}

static void prv_compass_handler(CompassHeadingData heading_data) {
  s_heading = heading_data.magnetic_heading;
  s_heading_valid =
      heading_data.compass_status != CompassStatusDataInvalid;
  prv_update_status();
  layer_mark_dirty(s_dial_layer);
}

void compass_window_on_location(void) {
  if (!s_window || !window_stack_contains_window(s_window)) {
    return;
  }
  prv_update_distance();
  prv_update_status();
  layer_mark_dirty(s_dial_layer);
}

static void prv_window_appear(Window *window) {
  compass_service_subscribe(prv_compass_handler);
  s_ping_radius = -1;
  s_ping_timer = app_timer_register(600, prv_ping_tick, NULL);
}

static void prv_window_disappear(Window *window) {
  compass_service_unsubscribe();
  if (s_ping_timer) {
    app_timer_cancel(s_ping_timer);
    s_ping_timer = NULL;
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_title_layer = text_layer_create(GRect(4, 0, bounds.size.w - 8, TITLE_H));
  // System Gothic covers Latin only; swap for the bundled font on Cyrillic
  text_layer_set_font(s_title_layer,
                      g_cyrillic
                          ? data_title_font()
                          : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_title_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_title_layer, s_article.title);
  layer_add_child(root, text_layer_get_layer(s_title_layer));
#ifdef PBL_ROUND
  // Flow the title inside the circle instead of clipping at the bezel
  text_layer_enable_screen_text_flow_and_paging(s_title_layer, 4);
#endif

  int dial_h = bounds.size.h - TITLE_H - DIST_H;
  s_dial_layer = layer_create(GRect(0, TITLE_H, bounds.size.w, dial_h));
  layer_set_update_proc(s_dial_layer, prv_dial_update);
  layer_add_child(root, s_dial_layer);

  s_status_layer = text_layer_create(
      GRect(8, TITLE_H + dial_h / 2 - 24, bounds.size.w - 16, 48));
  text_layer_set_font(s_status_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_status_layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(s_status_layer));

  s_dist_layer = text_layer_create(
      GRect(0, bounds.size.h - DIST_H, bounds.size.w, DIST_H));
  text_layer_set_font(s_dist_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_dist_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_dist_layer));

  // Scale the arrow to the dial this screen actually gives us
  int radius = prv_dial_radius();
  for (int i = 0; i < ARROW_POINTS; i++) {
    s_arrow_points[i] = GPoint(ARROW_BASE[i].x * radius / ARROW_BASE_RADIUS,
                               ARROW_BASE[i].y * radius / ARROW_BASE_RADIUS);
  }
  s_arrow = gpath_create(&ARROW_PATH_INFO);
  prv_update_distance();
  prv_update_status();
}

static void prv_window_unload(Window *window) {
  gpath_destroy(s_arrow);
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_dist_layer);
  text_layer_destroy(s_status_layer);
  layer_destroy(s_dial_layer);
  window_destroy(s_window);
  s_window = NULL;
}

void compass_window_push(const Article *article) {
  s_article = *article;
  s_heading_valid = false;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_window_load,
      .unload = prv_window_unload,
      .appear = prv_window_appear,
      .disappear = prv_window_disappear,
  });
  window_stack_push(s_window, true);
}
