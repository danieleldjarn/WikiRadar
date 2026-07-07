#include "data.h"

Article g_articles[MAX_ARTICLES];
int g_article_count = 0;
char g_summary[MAX_SUMMARY_LEN];
bool g_units_imperial = false;

// Persist keys 1..50 belong to cache.c
#define PKEY_UNITS 60

void data_set_units(bool imperial) {
  g_units_imperial = imperial;
  persist_write_bool(PKEY_UNITS, imperial);
}

void data_load_units(void) {
  g_units_imperial = persist_read_bool(PKEY_UNITS);
}
int32_t g_cur_lat = 0;
int32_t g_cur_lon = 0;
bool g_has_location = false;
time_t g_list_fetch_time = 0;

// Equirectangular deltas from current location to the article, in units of
// 1e-5 degrees of latitude (~1.11 m each); dx east, dy north.
static void prv_deltas(const Article *a, int32_t *out_dx, int32_t *out_dy) {
  int32_t dlat = a->lat - g_cur_lat;
  int32_t dlon = a->lon - g_cur_lon;
  int32_t lat_angle =
      (int32_t)((int64_t)g_cur_lat * TRIG_MAX_ANGLE / 36000000);
  int32_t coslat = cos_lookup(lat_angle);
  *out_dx = (int32_t)((int64_t)dlon * coslat / TRIG_MAX_RATIO);
  *out_dy = dlat;
}

static int64_t prv_isqrt(int64_t v) {
  if (v <= 0) {
    return 0;
  }
  int64_t x = v, y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + v / x) / 2;
  }
  return x;
}

int32_t data_distance_to(const Article *a) {
  if (!g_has_location) {
    return a->distance_m;
  }
  int32_t dx, dy;
  prv_deltas(a, &dx, &dy);
  int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy;
  return (int32_t)(prv_isqrt(d2) * 111 / 100);
}

int32_t data_bearing_to(const Article *a) {
  int32_t dx, dy;
  prv_deltas(a, &dx, &dy);
  while (dx > INT16_MAX || dx < INT16_MIN || dy > INT16_MAX ||
         dy < INT16_MIN) {
    dx /= 2;
    dy /= 2;
  }
  if (dx == 0 && dy == 0) {
    return 0;
  }
  return atan2_lookup(dx, dy);
}

void data_format_distance(int32_t meters, char *buf, size_t buf_len) {
  if (g_units_imperial) {
    int32_t feet = meters * 328 / 100;
    if (feet < 1000) {
      snprintf(buf, buf_len, "%d ft", (int)feet);
    } else {
      int32_t tenth_miles = meters * 10 / 1609;
      snprintf(buf, buf_len, "%d.%d mi", (int)(tenth_miles / 10),
               (int)(tenth_miles % 10));
    }
  } else {
    if (meters < 1000) {
      snprintf(buf, buf_len, "%d m", (int)meters);
    } else {
      int32_t tenth_kms = meters / 100;
      snprintf(buf, buf_len, "%d.%d km", (int)(tenth_kms / 10),
               (int)(tenth_kms % 10));
    }
  }
}
