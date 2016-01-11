#include "pebble.h"

#define BT_CONNECTION_STATUS_KEY 1

#define BT_CONNECTION_STATUS_DEFAULT false

#define KEY_REQUEST_BATTERY 1
#define KEY_BATTERY_LEVEL 0

static Window *window;
static BitmapLayer *bitmap_bluetooth_layer;
static GBitmap *bitmap_bluetooth;
static TextLayer *text_phone_battery_layer;
static TextLayer *text_battery_layer;
static TextLayer *text_day_layer;
static TextLayer *text_date_layer;
static TextLayer *text_time_layer;
static TextLayer *text_ampm_layer;
static Layer *line_layer;
static bool bt_connection_status = false;
static const uint32_t segments[] = {2000};

static char day_text[] = "Xxxxxxxxx";
static char date_text[] = "Xxxxxxxxx 00";
static char time_text[] = "00:00";
static char ampm_text[] = "XX";
static char battery_text[] = "100%";
static char phone_battery_text[] = "100%";
static char *time_format;

static void send(int key, int value) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  dict_write_int(iter, key, &value, sizeof(int), true);

  app_message_outbox_send();
}

static void line_layer_update_callback(Layer *layer, GContext* ctx) {
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void handle_bluetooth(bool connected) {
	if (bt_connection_status != connected) {
		if (connected) {
			bitmap_layer_set_bitmap(bitmap_bluetooth_layer, bitmap_bluetooth);
      
      send(KEY_REQUEST_BATTERY, 0);
		} else {
			VibePattern pattern = {
				.durations = segments,
				.num_segments = ARRAY_LENGTH(segments),
			};
			vibes_enqueue_custom_pattern(pattern);

			bitmap_layer_set_bitmap(bitmap_bluetooth_layer, NULL);
      text_layer_set_text(text_phone_battery_layer, NULL);
		}
		
		bt_connection_status = connected;
	}
}

static void handle_battery(BatteryChargeState charge_state) {
	snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
	text_layer_set_text(text_battery_layer, battery_text);
}

static void receive_data_handler(DictionaryIterator *iterator, void *context) {
  Tuple *result_tuple = dict_find(iterator, KEY_BATTERY_LEVEL);
  snprintf(phone_battery_text, sizeof(phone_battery_text), "%d%%", (int) result_tuple->value->int32);
	text_layer_set_text(text_phone_battery_layer, phone_battery_text);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {

	if (!tick_time) {
		time_t now = time(NULL);
		tick_time = localtime(&now);
	}

	if (units_changed >= DAY_UNIT) {
		strftime(day_text, sizeof(day_text), "%A", tick_time);
		text_layer_set_text(text_day_layer, day_text);

		strftime(date_text, sizeof(date_text), "%B %e", tick_time);
		text_layer_set_text(text_date_layer, date_text);
	}
  
  if (!clock_is_24h_style()) {
    strftime(ampm_text, sizeof(ampm_text), "%p", tick_time);
    text_layer_set_text(text_ampm_layer, ampm_text);
  }
  
	strftime(time_text, sizeof(time_text), time_format, tick_time);
	if (!clock_is_24h_style() && (time_text[0] == '0')) {
		memmove(time_text, &time_text[1], sizeof(time_text) - 1);
	}

	text_layer_set_text(text_time_layer, time_text);
  
  if (bt_connection_status) {
    send(KEY_REQUEST_BATTERY, 0);
  }
}

static void handle_init(void) {
	window = window_create();
	window_stack_push(window, true);
	window_set_background_color(window, GColorBlack);

	Layer *window_layer = window_get_root_layer(window);

	GFont font_square_medium = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_19));
  GFont font_square_large;
  
	bitmap_bluetooth = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_ICON);
	bitmap_bluetooth_layer = bitmap_layer_create(GRect(10, 9, 10, 13));
	layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_bluetooth_layer));
  
  text_phone_battery_layer = text_layer_create(GRect(25, 3, 50, 21));
	text_layer_set_text_color(text_phone_battery_layer, GColorWhite);
	text_layer_set_background_color(text_phone_battery_layer, GColorClear);
	text_layer_set_font(text_phone_battery_layer, font_square_medium);
	layer_add_child(window_layer, text_layer_get_layer(text_phone_battery_layer));
  
	text_battery_layer = text_layer_create(GRect(5, 3, 134, 21));
	text_layer_set_text_color(text_battery_layer, GColorWhite);
	text_layer_set_background_color(text_battery_layer, GColorClear);
	text_layer_set_text_alignment(text_battery_layer, GTextAlignmentRight);
	text_layer_set_font(text_battery_layer, font_square_medium);
	layer_add_child(window_layer, text_layer_get_layer(text_battery_layer));

	text_day_layer = text_layer_create(GRect(5, 30, 134, 21));
	text_layer_set_text_color(text_day_layer, GColorWhite);
	text_layer_set_background_color(text_day_layer, GColorClear);
	text_layer_set_text_alignment(text_day_layer, GTextAlignmentRight);
	text_layer_set_font(text_day_layer, font_square_medium);
	layer_add_child(window_layer, text_layer_get_layer(text_day_layer));

	text_date_layer = text_layer_create(GRect(5, 48, 134, 21));
	text_layer_set_text_color(text_date_layer, GColorWhite);
	text_layer_set_background_color(text_date_layer, GColorClear);
	text_layer_set_text_alignment(text_date_layer, GTextAlignmentRight);
	text_layer_set_font(text_date_layer, font_square_medium);
	layer_add_child(window_layer, text_layer_get_layer(text_date_layer));
  
  if (clock_is_24h_style()) {
    time_format = "%R";
    font_square_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_51));
  } else {
	  text_ampm_layer = text_layer_create(GRect(5, 130, 134, 21));
	  text_layer_set_text_color(text_ampm_layer, GColorWhite);
	  text_layer_set_background_color(text_ampm_layer, GColorClear);
	  text_layer_set_text_alignment(text_ampm_layer, GTextAlignmentRight);
	  text_layer_set_font(text_ampm_layer, font_square_medium);
	  layer_add_child(window_layer, text_layer_get_layer(text_ampm_layer));
    
		time_format = "%I:%M";
    font_square_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_58));
  }
  
  text_time_layer = text_layer_create(GRect(5, 72, 134, 60));
	text_layer_set_text_color(text_time_layer, GColorWhite);
	text_layer_set_background_color(text_time_layer, GColorClear);
	text_layer_set_text_alignment(text_time_layer, GTextAlignmentRight);
	text_layer_set_font(text_time_layer, font_square_large);
	layer_add_child(window_layer, text_layer_get_layer(text_time_layer));
  
	GRect line_frame = GRect(8, 77, 139, 2);
	line_layer = layer_create(line_frame);
	layer_set_update_proc(line_layer, line_layer_update_callback);
	layer_add_child(window_layer, line_layer);

	bt_connection_status = persist_exists(BT_CONNECTION_STATUS_KEY) ? persist_read_int(BT_CONNECTION_STATUS_KEY) : BT_CONNECTION_STATUS_DEFAULT;
  
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = handle_bluetooth
  });
	handle_bluetooth(connection_service_peek_pebble_app_connection());
  
  app_message_register_inbox_received(receive_data_handler);
  app_message_open(12, 12);
  
	battery_state_service_subscribe(handle_battery);
	handle_battery(battery_state_service_peek());

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
	handle_minute_tick(NULL, YEAR_UNIT);
}

static void handle_deinit(void) {
	connection_service_unsubscribe();
	battery_state_service_unsubscribe();
	tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();

  persist_write_bool(BT_CONNECTION_STATUS_KEY, bt_connection_status);
  
	gbitmap_destroy(bitmap_bluetooth);
  bitmap_layer_destroy(bitmap_bluetooth_layer);
	text_layer_destroy(text_battery_layer);
	text_layer_destroy(text_date_layer);
	text_layer_destroy(text_time_layer);
  if(!clock_is_24h_style()) {
	  text_layer_destroy(text_ampm_layer);
  }
  layer_destroy(line_layer);
  
	window_destroy(window);
}

int main(void) {
	handle_init();

	app_event_loop();

	handle_deinit();
}
