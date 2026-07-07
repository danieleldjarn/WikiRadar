#include <pebble.h>

// CMD values shared with the JS side
#define CMD_PING 0
#define CMD_PONG 1

static Window *s_window;
static TextLayer *s_status_layer;

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *cmd = dict_find(iter, MESSAGE_KEY_CMD);
  if (!cmd) {
    return;
  }
  if (cmd->value->int32 == CMD_PING) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Ping received from phone");
    text_layer_set_text(s_status_layer, "Phone connected");

    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) == APP_MSG_OK) {
      dict_write_uint8(out, MESSAGE_KEY_CMD, CMD_PONG);
      app_message_outbox_send();
    }
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_status_layer = text_layer_create(
      GRect(0, bounds.size.h / 2 - 20, bounds.size.w, 40));
  text_layer_set_text(s_status_layer, "Waiting for phone...");
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_font(s_status_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(root, text_layer_get_layer(s_status_layer));
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_status_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_window_load,
      .unload = prv_window_unload,
  });

  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_open(1024, 1024);

  window_stack_push(s_window, true);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
