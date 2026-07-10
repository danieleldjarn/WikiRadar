#include "data.h"

Article g_articles[MAX_ARTICLES];
int g_article_count = 0;
char g_summary[MAX_SUMMARY_LEN];
bool g_units_imperial = false;

// Persist keys 1..50 belong to cache.c
#define PKEY_UNITS 60
#define PKEY_READ_SET 61

void data_set_units(bool imperial) {
  g_units_imperial = imperial;
  persist_write_bool(PKEY_UNITS, imperial);
}

void data_load_units(void) {
  g_units_imperial = persist_read_bool(PKEY_UNITS);
}

// 0 = follow the watch-wide content size; 1/2/3 = small/medium/large
#define PKEY_TEXT_SIZE 62
#define PKEY_CYRILLIC 63
static int s_text_size = 0;
bool g_cyrillic = false;

void data_set_cyrillic(bool cyrillic) {
  if (cyrillic == g_cyrillic) {
    return;
  }
  g_cyrillic = cyrillic;
  persist_write_bool(PKEY_CYRILLIC, cyrillic);
}

void data_load_cyrillic(void) {
  g_cyrillic = persist_read_bool(PKEY_CYRILLIC);
}

static GFont prv_custom_font(uint32_t resource_id, GFont *cache) {
  if (!*cache) {
    *cache = fonts_load_custom_font(resource_get_handle(resource_id));
  }
  return *cache;
}

GFont data_title_font(void) {
  static GFont s_title_font;
  if (g_cyrillic) {
    return prv_custom_font(RESOURCE_ID_FONT_TITLE_22, &s_title_font);
  }
  return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
}

void data_set_text_size(int pref) {
  if (pref < 0 || pref > 4) {
    return;
  }
  s_text_size = pref;
  persist_write_int(PKEY_TEXT_SIZE, pref);
}

void data_load_text_size(void) {
  s_text_size = persist_read_int(PKEY_TEXT_SIZE);  // 0 when unset
}

GFont data_body_font(void) {
  static GFont s_xl_font;
  int effective = s_text_size;
  if (effective == 0) {
#if PBL_API_EXISTS(preferred_content_size)
    switch (preferred_content_size()) {
      case PreferredContentSizeSmall:
        effective = 1;
        break;
      case PreferredContentSizeLarge:
        effective = 3;
        break;
      case PreferredContentSizeExtraLarge:
        effective = 4;
        break;
      default:
        effective = 2;
    }
#else
    effective = 2;
#endif
  }
  if (effective == 4) {
    // Bundled DejaVu Sans Bold: big, heavy, and with Latin + Cyrillic
    // coverage (Bitham renders non-ASCII letters as tiny fallbacks)
    return prv_custom_font(RESOURCE_ID_FONT_XL_30, &s_xl_font);
  }
  if (g_cyrillic) {
    static GFont s_cyr_fonts[3];  // 18 / 24 / 28
    switch (effective) {
      case 1:
        return prv_custom_font(RESOURCE_ID_FONT_BODY_18, &s_cyr_fonts[0]);
      case 3:
        return prv_custom_font(RESOURCE_ID_FONT_BODY_28, &s_cyr_fonts[2]);
      default:
        return prv_custom_font(RESOURCE_ID_FONT_BODY_24, &s_cyr_fonts[1]);
    }
  }
  switch (effective) {
    case 1:
      return fonts_get_system_font(FONT_KEY_GOTHIC_18);
    case 3:
      return fonts_get_system_font(FONT_KEY_GOTHIC_28);
    default:
      return fonts_get_system_font(FONT_KEY_GOTHIC_24);
  }
}

// Rolling set of FNV-1a title hashes; oldest entries get overwritten.
#define READ_SET_SIZE 30
typedef struct {
  uint8_t next;
  uint32_t hashes[READ_SET_SIZE];
} ReadSet;  // 124 bytes, fits one persist value

static ReadSet s_read_set;

static uint32_t prv_title_hash(const char *s) {
  uint32_t h = 2166136261u;
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 16777619u;
  }
  return h;
}

bool data_is_read(const char *title) {
  uint32_t h = prv_title_hash(title);
  for (int i = 0; i < READ_SET_SIZE; i++) {
    if (s_read_set.hashes[i] == h) {
      return true;
    }
  }
  return false;
}

void data_mark_read(const char *title) {
  if (data_is_read(title)) {
    return;
  }
  s_read_set.hashes[s_read_set.next] = prv_title_hash(title);
  s_read_set.next = (s_read_set.next + 1) % READ_SET_SIZE;
  persist_write_data(PKEY_READ_SET, &s_read_set, sizeof(s_read_set));
}

void data_load_read_set(void) {
  persist_read_data(PKEY_READ_SET, &s_read_set, sizeof(s_read_set));
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
