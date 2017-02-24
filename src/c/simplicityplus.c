#include <pebble.h>

#define KEY_BT_CONNECTION_STATUS 1
#define KEY_PHONE_BATTERY 2

#define DEFAULT_BT_CONNECTION_STATUS false
#define DEFAULT_PHONE_BATTERY -1

#define KEY_BATTERY_LEVEL 0
#define KEY_REQUEST_BATTERY 1

// Wait time for pinging for phone battery
#define INTERVAL_PHONE_BATTERY 600000 // 60 * 10 * 1000 = 10 minutes
#define INTERVAL_STANDARD_WAIT 2000

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

static bool bt_connection_status;
static int phone_battery;
static const uint32_t segments[] = {2000};

static GColor text_color;

static char day_text[] = "Xxxxxxxxx";
static char date_text[] = "Xxxxxxxxx 00";
static char time_text[] = "00:00";
static char ampm_text[] = "XX";
static char battery_text[] = "1XX%";
static char phone_battery_text[] = "1XX%";
static char *time_format;

static AppTimer *timer_phone_battery;

static GColor get_battery_color(int percentage);

static void handle_battery_timer(void* data);
static void line_layer_update_callback(Layer *layer, GContext* ctx);
static void handle_bluetooth(bool connected);
static void handle_battery(BatteryChargeState charge_state);
static void receive_data_handler(DictionaryIterator *iterator, void *context);
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);
static void handle_init(void);
static void handle_deinit(void);

static void handle_battery_timer(void* data) {
  if (bt_connection_status) {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
  
    dict_write_int32(iter, KEY_REQUEST_BATTERY, 0);
  
    int send_response = app_message_outbox_send();
    
    if (send_response == APP_MSG_BUSY) {
      app_message_deregister_callbacks();
      app_message_register_inbox_received(receive_data_handler);
      app_message_open(12, 12);
    }
  }
  
  timer_phone_battery = app_timer_register(INTERVAL_PHONE_BATTERY, handle_battery_timer, NULL);
}

static void line_layer_update_callback(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, text_color);
	graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void handle_bluetooth(bool connected) {
  // Check if we have already handled this change
  if (connected != bt_connection_status) {
    
    if (connected) {
      // Update the bluetooth icon
      bitmap_layer_set_bitmap(bitmap_bluetooth_layer, bitmap_bluetooth);
      
      // Update the phone battery if it is known
      if (phone_battery >= 0) {
        text_layer_set_text(text_phone_battery_layer, phone_battery_text);
      }
      
      // Cancel the phone battery timer if it exists
      if (timer_phone_battery) {
        app_timer_cancel(timer_phone_battery);
      }
      
      // Start the phone battery update timer
      timer_phone_battery = app_timer_register(INTERVAL_STANDARD_WAIT, handle_battery_timer, NULL);
      
    } else {
      
      // Vibrate on disconnect
      VibePattern pattern = {
        .durations = segments,
        .num_segments = ARRAY_LENGTH(segments),
      };
      vibes_enqueue_custom_pattern(pattern);
      
      // The phone is disconnected, update the icons
      bitmap_layer_set_bitmap(bitmap_bluetooth_layer, NULL);
      text_layer_set_text(text_phone_battery_layer, NULL);
    }
    
    // Update the handled connection
    bt_connection_status = connected;
  }
}

static GColor get_battery_color(int percentage) {
  #if defined(PBL_COLOR)
    if (percentage >= 70) {
      return GColorMintGreen;
    } else if (percentage >= 30) {
      return GColorPastelYellow;
    } else {
      return GColorMelon;
    }
  #else
    return GColorWhite;
  #endif
}

static void handle_battery(BatteryChargeState charge_state) {
  
  text_layer_set_text_color(text_battery_layer, get_battery_color(charge_state.charge_percent));
  
  // Update the battery text
	snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
	text_layer_set_text(text_battery_layer, battery_text);
}

static void receive_data_handler(DictionaryIterator *iterator, void *context) {
  Tuple *result_tuple = dict_find(iterator, KEY_BATTERY_LEVEL);
  
  phone_battery = (int) result_tuple->value->int32;
  
  text_layer_set_text_color(text_phone_battery_layer, get_battery_color(phone_battery));
  
  snprintf(phone_battery_text, sizeof(phone_battery_text), "%d%%", phone_battery);
	text_layer_set_text(text_phone_battery_layer, phone_battery_text);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {

	if (!tick_time) {
		time_t now = time(NULL);
		tick_time = localtime(&now);
    app_timer_cancel(timer_phone_battery);
    timer_phone_battery = app_timer_register(INTERVAL_STANDARD_WAIT, handle_battery_timer, NULL);
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
}

static void handle_init(void) {
	window = window_create();
	window_stack_push(window, true);
	window_set_background_color(window, GColorBlack);

	Layer *window_layer = window_get_root_layer(window);

	GFont font_square_medium = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_19));
  GFont font_square_large;
  
  text_color = PBL_IF_COLOR_ELSE(GColorPastelYellow, GColorWhite);
  
  bt_connection_status = persist_exists(KEY_BT_CONNECTION_STATUS) ? persist_read_int(KEY_BT_CONNECTION_STATUS) : DEFAULT_BT_CONNECTION_STATUS;
  
  phone_battery = persist_exists(KEY_PHONE_BATTERY) ? persist_read_int(KEY_PHONE_BATTERY) : DEFAULT_PHONE_BATTERY;
  
  if (phone_battery >= 0) {
    snprintf(phone_battery_text, sizeof(phone_battery_text), "%d%%", phone_battery);
  }
  
	bitmap_bluetooth = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_ICON);
	bitmap_bluetooth_layer = bitmap_layer_create(GRect(10, 9, 10, 13));
	layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_bluetooth_layer));
  
  text_phone_battery_layer = text_layer_create(GRect(25, 3, 50, 21));
  text_layer_set_text_color(text_phone_battery_layer, get_battery_color(phone_battery));
	text_layer_set_background_color(text_phone_battery_layer, GColorClear);
	text_layer_set_font(text_phone_battery_layer, font_square_medium);
	layer_add_child(window_layer, text_layer_get_layer(text_phone_battery_layer));
  
	text_battery_layer = text_layer_create(GRect(5, 3, 134, 21));
	text_layer_set_background_color(text_battery_layer, GColorClear);
	text_layer_set_text_alignment(text_battery_layer, GTextAlignmentRight);
	text_layer_set_font(text_battery_layer, font_square_medium);
	layer_add_child(window_layer, text_layer_get_layer(text_battery_layer));

	text_day_layer = text_layer_create(GRect(5, 30, 134, 21));
	text_layer_set_text_color(text_day_layer, text_color);
	text_layer_set_background_color(text_day_layer, GColorClear);
	text_layer_set_text_alignment(text_day_layer, GTextAlignmentRight);
	text_layer_set_font(text_day_layer, font_square_medium);
	layer_add_child(window_layer, text_layer_get_layer(text_day_layer));

	text_date_layer = text_layer_create(GRect(5, 48, 134, 21));
	text_layer_set_text_color(text_date_layer, text_color);
	text_layer_set_background_color(text_date_layer, GColorClear);
	text_layer_set_text_alignment(text_date_layer, GTextAlignmentRight);
	text_layer_set_font(text_date_layer, font_square_medium);
	layer_add_child(window_layer, text_layer_get_layer(text_date_layer));
  
  if (clock_is_24h_style()) {
    time_format = "%R";
    font_square_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_51));
  } else {
	  text_ampm_layer = text_layer_create(GRect(5, 130, 134, 21));
	  text_layer_set_text_color(text_ampm_layer, text_color);
	  text_layer_set_background_color(text_ampm_layer, GColorClear);
	  text_layer_set_text_alignment(text_ampm_layer, GTextAlignmentRight);
	  text_layer_set_font(text_ampm_layer, font_square_medium);
	  layer_add_child(window_layer, text_layer_get_layer(text_ampm_layer));
    
		time_format = "%I:%M";
    font_square_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_58));
  }
  
  text_time_layer = text_layer_create(GRect(5, 72, 134, 60));
	text_layer_set_text_color(text_time_layer, text_color);
	text_layer_set_background_color(text_time_layer, GColorClear);
	text_layer_set_text_alignment(text_time_layer, GTextAlignmentRight);
	text_layer_set_font(text_time_layer, font_square_large);
	layer_add_child(window_layer, text_layer_get_layer(text_time_layer));
  
	GRect line_frame = GRect(8, 77, 139, 2);
	line_layer = layer_create(line_frame);
	layer_set_update_proc(line_layer, line_layer_update_callback);
	layer_add_child(window_layer, line_layer);
  
  app_message_register_inbox_received(receive_data_handler);
  app_message_open(12, 12);
  
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = handle_bluetooth
  });
	handle_bluetooth(connection_service_peek_pebble_app_connection());
  
	battery_state_service_subscribe(handle_battery);
	handle_battery(battery_state_service_peek());

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
	handle_minute_tick(NULL, YEAR_UNIT);
}

static void handle_deinit(void) {
  // Unsubscribe from events
	connection_service_unsubscribe();
	battery_state_service_unsubscribe();
	tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  
  // Store status for when app is reloaded
  persist_write_bool(KEY_BT_CONNECTION_STATUS, bt_connection_status);
  persist_write_int(KEY_PHONE_BATTERY, phone_battery);
  
  // Destroy layers
	gbitmap_destroy(bitmap_bluetooth);
  bitmap_layer_destroy(bitmap_bluetooth_layer);
	text_layer_destroy(text_battery_layer);
	text_layer_destroy(text_date_layer);
	text_layer_destroy(text_time_layer);
  if(!clock_is_24h_style()) {
	  text_layer_destroy(text_ampm_layer);
  }
  layer_destroy(line_layer);
  
  // Destroy window
	window_destroy(window);
}

int main(void) {
	handle_init();

	app_event_loop();

	handle_deinit();
}