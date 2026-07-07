#include "data.h"

Article g_articles[MAX_ARTICLES];
int g_article_count = 0;
char g_summary[MAX_SUMMARY_LEN];
bool g_units_imperial = false;

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
