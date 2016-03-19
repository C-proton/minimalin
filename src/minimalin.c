#include <pebble.h>
#include "common.h"

#define HOUR_HAND_COLOR GColorRed

#ifdef PBL_ROUND
static GPoint ticks_points[12][2] = {
  {{90, 0}  , {90, 6}  },
  {{135,12} , {132,18}  },
  {{168,45} , {162,48} },
  {{180,90} , {174,90} },
  {{168,135}, {162,132}},
  {{135,168}, {132,162}},
  {{90, 180}, {90, 174}},
  {{45, 168}, {48, 162}},
  {{12, 135}, {18, 132}},
  {{0,  90} , {6,  90} },
  {{12, 45} , {18, 48} },
  {{45, 12} , {48, 18}  }
};
#else
static GPoint ticks_points[12][2] = {
  {{72, 0}  , {72, 7}  },
  {{120,0}  , {117,7}  },
  {{144,42} , {137,46} },
  {{144,84} , {137,84} },
  {{144,126}, {137,122}},
  {{120,168}, {117,161}},
  {{72, 168}, {72, 161}},
  {{24, 168}, {27, 161}},
  {{0,  126}, {7,  122}},
  {{0,  84} , {7,  84} },
  {{0,  42} , {7,  46} },
  {{24, 0}  , {27, 7}  }
};
#endif
#ifdef PBL_ROUND
static GPoint time_points[12] = {
  {90,  21} ,
  {124, 30} ,
  {150, 56} ,
  {159, 90} ,
  {150, 124},
  {124, 150},
  {90,  159},
  {56,  150},
  {30,  124},
  {21,  90} ,
  {30,  56} ,
  {56,  30} ,
};
#else
static GPoint time_points[12] = {
  {72,  15} ,
  {112, 15} ,
  {126, 47} ,
  {126, 82},
  {126, 117},
  {112, 145} ,
  {72,  145},
  {32,  145},
  {18,  117},
  {18,  82} ,
  {18,  47} ,
  {32,  15} ,
};
#endif

typedef void (*ConfigUpdatedCallback)();
typedef enum { NoIcon = 0, Bluetooth = 1, Heart = 2 } BluetoothIcon;
typedef enum { Hour, Minute } TimeType;
typedef struct {
  int32_t minute_hand_color;
  int32_t hour_hand_color;
  int32_t background_color;
  int32_t date_color;
  int32_t time_color;
  int8_t date_displayed;
  int8_t bluetooth_icon;
  int8_t rainbow_mode;
} __attribute__((__packed__)) Config;

static const int MESSAGE_KEY_MINUTE_HAND_COLOR = 0;
static const int MESSAGE_KEY_HOUR_HAND_COLOR   = 1;
static const int MESSAGE_KEY_DATE_DISPLAYED    = 2;
static const int MESSAGE_KEY_BLUETOOTH_ICON    = 3;
static const int MESSAGE_KEY_RAINBOW_MODE      = 4;
static const int MESSAGE_KEY_BACKGROUND_COLOR  = 5;
static const int MESSAGE_KEY_DATE_COLOR        = 6;
static const int MESSAGE_KEY_TIME_COLOR        = 7;
static const int PERSIST_KEY_CONFIG            = 0;

static const int HOUR_CIRCLE_RADIUS = 5;
static const int HOUR_HAND_STROKE = 6;
static const int HOUR_HAND_RADIUS = 39;
static const int MINUTE_HAND_STROKE = 6;
static const int MINUTE_HAND_RADIUS = 52;
static const int ICON_OFFSET = -18;
static const int TICK_STROKE = 2;
static const int TICK_LENGTH = 6;
static const int DATE_Y_OFFSET = 28;

static Window * s_main_window;
static TextLayer * s_bt_layer;
static Layer * s_root_layer;
static Layer * s_tick_layer;
static GRect s_bounds;
static GPoint s_center;

static GPoint s_center;
static GRect s_bounds;
static GBitmap * s_rainbow_bitmap;
static Layer * s_minute_hand_layer;
static Layer * s_hour_hand_layer;
static RotBitmapLayer * s_rainbow_hand_layer;
static Layer * s_center_circle_layer;
static GRect s_bounds;
static GPoint s_center;
static Layer * s_time_layer;

static Config s_config;
static ConfigUpdatedCallback s_config_updated_callback;

static void fetch_int32(const DictionaryIterator * iter, const int key, int32_t * config){
  Tuple * tuple = dict_find(iter, key);
  if(tuple){
    *config = tuple->value->int32;
  }
}

static void fetch_int8(const DictionaryIterator * iter, const int key, int8_t * config){
  Tuple * tuple = dict_find(iter, key);
  if(tuple){
    *config = tuple->value->int8;
  }
}

static void fetch_config_or_default(){
  if(persist_exists(PERSIST_KEY_CONFIG)){
    persist_read_data(PERSIST_KEY_CONFIG, &s_config, sizeof(Config));
  }else{
    s_config.minute_hand_color = 0xffffff;
    s_config.hour_hand_color   = 0xff0000;
    s_config.background_color  = 0x000000;
    s_config.date_color        = 0x555555;
    s_config.time_color        = 0xAAAAAA;
    s_config.date_displayed    = true;
    s_config.bluetooth_icon    = Bluetooth;
    s_config.rainbow_mode      = false;
    persist_write_data(PERSIST_KEY_CONFIG, &s_config, sizeof(s_config));
  }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  fetch_int32(iter, MESSAGE_KEY_MINUTE_HAND_COLOR, &s_config.minute_hand_color);
  fetch_int32(iter, MESSAGE_KEY_HOUR_HAND_COLOR, &s_config.hour_hand_color);
  fetch_int32(iter, MESSAGE_KEY_BACKGROUND_COLOR, &s_config.background_color);
  fetch_int32(iter, MESSAGE_KEY_DATE_COLOR, &s_config.date_color);
  fetch_int32(iter, MESSAGE_KEY_TIME_COLOR, &s_config.time_color);
  fetch_int8(iter, MESSAGE_KEY_DATE_DISPLAYED, &s_config.date_displayed);
  fetch_int8(iter, MESSAGE_KEY_BLUETOOTH_ICON, &s_config.bluetooth_icon);
  fetch_int8(iter, MESSAGE_KEY_RAINBOW_MODE, &s_config.rainbow_mode);
  persist_write_data(PERSIST_KEY_CONFIG, &s_config, sizeof(s_config));
  s_config_updated_callback();
}

static GColor config_get_minute_hand_color(){
  return GColorFromHEX(s_config.minute_hand_color);
}

static GColor config_get_hour_hand_color(){
  return GColorFromHEX(s_config.hour_hand_color);
}

static GColor config_get_background_color(){
  return GColorFromHEX(s_config.background_color);
}

static GColor config_get_date_color(){
  return GColorFromHEX(s_config.date_color);
}

static GColor config_get_time_color(){
  return GColorFromHEX(s_config.time_color);
}

static bool config_is_date_displayed(){
  return s_config.date_displayed;
}

static BluetoothIcon config_get_bluetooth_icon(){
  return s_config.bluetooth_icon;
}

static bool config_is_rainbow_mode(){
  return s_config.rainbow_mode;
}

static void init_config(ConfigUpdatedCallback callback){
  s_config_updated_callback = callback;
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  fetch_config_or_default();
}

// Hands
static bool times_overlap(const int hour, const int minute){
  return (hour == 12 && minute < 5) || hour == minute / 5;
}

static bool time_displayed_horizontally(const int hour, const int minute){
  return times_overlap(hour, minute) && (hour <= 1 || hour >= 11 || (hour <= 7 && hour >= 5));
}

static bool time_displayed_vertically(const int hour, const int minute){
  return times_overlap(hour, minute) && ((hour > 1 && hour < 5) || (hour > 7 && hour < 11));
}

static GSize get_display_box_size(const char * text){
  return graphics_text_layout_get_content_size(text, get_font(), s_bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter);
}

static GRect get_display_box(const GPoint box_center, const char * time){
  const GSize box_size    = get_display_box_size(time);
  const GPoint box_origin = GPoint(box_center.x - box_size.w / 2, box_center.y - box_size.h / 2);
  const GRect box         = (GRect) { .origin = box_origin, .size = box_size };
  return box;
}

static GPoint get_time_point(const int time, const TimeType type){
  if(type == Minute){
    return time_points[time / 5];
  }
  return time_points[time % 12];
}

static void display_number(GContext * ctx, const GRect box, const int number, const bool leading_zero){
  char buffer[] = "00";
  if(leading_zero){
    snprintf(buffer, sizeof(buffer), "%02d", number);
  }else{
    snprintf(buffer, sizeof(buffer), "%d", number);
  }
  draw_text(ctx, buffer, get_font(), box);
}

static void display_time(GContext * ctx, const int hour, const int minute){
  graphics_context_set_text_color(ctx, config_get_time_color());
  char buffer[] = "00:00";
  if(time_displayed_horizontally(hour, minute)){
    snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
    const GPoint box_center = get_time_point(hour, Hour);
    const GRect box         = get_display_box(box_center, "00:00");
    draw_text(ctx, buffer, get_font(), box);
  }else{
    GRect hour_box;
    GRect minute_box;
    if(time_displayed_vertically(hour, minute)){
      const GPoint box_center = get_time_point(hour, Hour);
      const GRect box         = get_display_box(box_center, "00");
      hour_box                = grect_translated(box, 0, - box.size.h / 2 - 4);
      minute_box              = grect_translated(box, 0, box.size.h / 2 - 2);
    }else{
      const GPoint hour_box_center   = get_time_point(hour, Hour);
      hour_box                       = get_display_box(hour_box_center, "00");
      const GPoint minute_box_center = get_time_point(minute, Minute);
      minute_box                     = get_display_box(minute_box_center, "00");
    }
    display_number(ctx, hour_box, hour, false);
    display_number(ctx, minute_box, minute, true);
  }
}

static void display_date(GContext * ctx, const int day){
  set_text_color(ctx, config_get_date_color());
  const GRect box = get_display_box(s_center, "00");
  display_number(ctx, grect_translated(box, 0, DATE_Y_OFFSET), day, false);
}

static void time_layer_update_callback(Layer * layer, GContext *ctx){
  Time current_time = get_current_time();
  display_time(ctx, current_time.hour, current_time.minute);
  if(config_is_date_displayed()){
    display_date(ctx, current_time.day);
  }
}

void init_times(Layer * root_layer){
  s_bounds = layer_get_bounds(root_layer);
  s_center = grect_center_point(&s_bounds);
  s_time_layer = layer_create(s_bounds);
  layer_set_update_proc(s_time_layer, time_layer_update_callback);
  layer_add_child(root_layer, s_time_layer);
}

void deinit_times(){
  layer_destroy(s_time_layer);
}

void mark_dirty_time_layer(){
  if(s_time_layer){
    layer_mark_dirty(s_time_layer);
  }
}

static void hands_update_time_changed(){
 if(s_hour_hand_layer){
   layer_mark_dirty(s_hour_hand_layer);
 }
 if(s_minute_hand_layer){
   layer_mark_dirty(s_minute_hand_layer);
 }
 if(s_rainbow_hand_layer){
   const Time current_time = get_current_time();
   const float hand_angle = angle(current_time.minute, 60);
   const bool rainbow_mode = config_is_rainbow_mode();
   rot_bitmap_layer_set_angle(s_rainbow_hand_layer, hand_angle);
   layer_set_hidden((Layer*)s_rainbow_hand_layer, !rainbow_mode);
 }
}

static void hands_update_minute_hand_config_changed(){
  if(s_minute_hand_layer){
    layer_mark_dirty(s_minute_hand_layer);
  }
}

static void hands_update_hour_hand_config_changed(){
  if(s_minute_hand_layer){
    layer_mark_dirty(s_minute_hand_layer);
  }
}

static void hands_update_rainbow_mode_config_changed(){
  const Time current_time = get_current_time();
  const float hand_angle = angle(current_time.minute, 60);
  const bool rainbow_mode = config_is_rainbow_mode();
  if(s_minute_hand_layer){
    layer_set_hidden(s_minute_hand_layer, rainbow_mode);
    layer_mark_dirty(s_minute_hand_layer);
  }
  if(s_rainbow_hand_layer){
    rot_bitmap_layer_set_angle(s_rainbow_hand_layer, hand_angle);
    layer_set_hidden((Layer*)s_rainbow_hand_layer, !rainbow_mode);
  }
  if(s_center_circle_layer){
    layer_mark_dirty(s_center_circle_layer);
  }
}


static void update_minute_hand_layer(Layer *layer, GContext * ctx){
  const Time current_time = get_current_time();
  const float hand_angle = angle(current_time.minute, 60);
  const GPoint hand_end = gpoint_on_circle(s_center, hand_angle, MINUTE_HAND_RADIUS);
  set_stroke_width(ctx, MINUTE_HAND_STROKE);
  set_stroke_color(ctx, config_get_minute_hand_color());
  draw_line(ctx, s_center, hand_end);
}

static void update_hour_hand_layer(Layer * layer, GContext * ctx){
  const Time current_time = get_current_time();
  const float hand_angle = angle(current_time.hour * 50 + current_time.minute * 50 / 60, 600);
  const GPoint hand_end = gpoint_on_circle(s_center, hand_angle, HOUR_HAND_RADIUS);
  set_stroke_width(ctx, HOUR_HAND_STROKE);
  set_stroke_color(ctx, config_get_hour_hand_color());
  draw_line(ctx, s_center, hand_end);
}

static void update_center_circle_layer(Layer * layer, GContext * ctx){
  if(config_is_rainbow_mode()){
    graphics_context_set_fill_color(ctx, GColorVividViolet);
  }else{
    graphics_context_set_fill_color(ctx, config_get_hour_hand_color());
  }
  graphics_fill_circle(ctx, s_center, HOUR_CIRCLE_RADIUS);
}

static void init_hands(Layer * root_layer){
  s_bounds = layer_get_bounds(root_layer);
  s_center = grect_center_point(&s_bounds);

  s_rainbow_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMG_RAINBOW_HAND);

  s_minute_hand_layer   = layer_create(s_bounds);
  s_hour_hand_layer     = layer_create(s_bounds);
  s_center_circle_layer = layer_create(s_bounds);
  s_rainbow_hand_layer  = rot_bitmap_layer_create(s_rainbow_bitmap);
  rot_bitmap_set_compositing_mode(s_rainbow_hand_layer, GCompOpSet);
  rot_bitmap_set_src_ic(s_rainbow_hand_layer, GPoint(5, 55));
  GRect frame = layer_get_frame((Layer *) s_rainbow_hand_layer);
  frame.origin.x = s_center.x - frame.size.w / 2;
  frame.origin.y = s_center.y - frame.size.h / 2;
  layer_set_frame((Layer *)s_rainbow_hand_layer, frame);

  layer_set_update_proc(s_hour_hand_layer,     update_hour_hand_layer);
  layer_set_update_proc(s_minute_hand_layer,   update_minute_hand_layer);
  layer_set_update_proc(s_center_circle_layer, update_center_circle_layer);

  layer_add_child(root_layer, s_minute_hand_layer);
  layer_add_child(root_layer, (Layer *)s_rainbow_hand_layer);
  layer_add_child(root_layer, s_hour_hand_layer);
  layer_add_child(root_layer, s_center_circle_layer);

  hands_update_rainbow_mode_config_changed();
}

static void deinit_hands(){
  layer_destroy(s_hour_hand_layer);
  rot_bitmap_layer_destroy(s_rainbow_hand_layer);
  layer_destroy(s_minute_hand_layer);
  layer_destroy(s_center_circle_layer);
  gbitmap_destroy(s_rainbow_bitmap);
}

// Ticks
static void mark_dirty_tick_layer();

static void draw_tick(GContext *ctx, const int index){
  draw_line(ctx, ticks_points[index][0], ticks_points[index][1]);
}

static void tick_layer_update_callback(Layer *layer, GContext *ctx) {
  const Time current_time = get_current_time();
  set_stroke_color(ctx, config_get_time_color());
  set_stroke_width(ctx, TICK_STROKE);
  const int hour_tick_index = current_time.hour % 12;
  draw_tick(ctx, hour_tick_index);
  const int minute_tick_index = current_time.minute / 5;
  if(hour_tick_index != minute_tick_index){
    draw_tick(ctx, minute_tick_index);
  }
}

static void init_tick_layer(Layer * root_layer){
  s_bounds = layer_get_bounds(root_layer);
  s_center = grect_center_point(&s_bounds);

  s_tick_layer = layer_create(s_bounds);
  layer_set_update_proc(s_tick_layer, tick_layer_update_callback);
  layer_add_child(root_layer, s_tick_layer);
}

static void deinit_tick_layer(){
  layer_destroy(s_tick_layer);
}

static void mark_dirty_tick_layer(){
  if(s_tick_layer){
    layer_mark_dirty(s_tick_layer);
  }
}


// Info layer: bluetooth

static void mark_dirty_info_layer();

static void bt_handler(bool connected){
  const GColor bg_color = config_get_background_color();
  bool bg_reddish = false;
  const GColor reddish_colors[] = { GColorRed, GColorFolly, GColorFashionMagenta, GColorMagenta };
  for(int i = 0; i < 4; i++){
    if(gcolor_equal(bg_color, reddish_colors[i])){
      bg_reddish = true;
      break;
    }
  }
  if(bg_reddish){
    text_layer_set_text_color(s_bt_layer, GColorWhite);
  }else{
    text_layer_set_text_color(s_bt_layer, GColorRed);
  }
  const BluetoothIcon new_icon = config_get_bluetooth_icon();
  if(connected || new_icon == NoIcon){
    text_layer_set_text(s_bt_layer, "");
  }else if(new_icon == Bluetooth){
    text_layer_set_text(s_bt_layer, "μ");
  }else{
    text_layer_set_text(s_bt_layer, "ν");
  }
}

static void mark_dirty_info_layer(){
  bt_handler(connection_service_peek_pebble_app_connection());
}

static void init_info_layer(Layer * root_layer){
  s_root_layer = root_layer;
  const GRect root_layer_bounds = layer_get_bounds(root_layer);
  const GPoint center = grect_center_point(&root_layer_bounds);
  const GSize size = GSize(23, 23);
  const GRect rect_at_center = (GRect) { .origin = center, .size = size };
  const GRect bounds = grect_translated(rect_at_center, - size.w / 2, - size.h + ICON_OFFSET);
  s_bt_layer = text_layer_create(bounds);
  text_layer_set_text_alignment(s_bt_layer, GTextAlignmentCenter);
  text_layer_set_font(s_bt_layer, get_font());
  text_layer_set_overflow_mode(s_bt_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_bt_layer, GColorClear);
  layer_add_child(root_layer, text_layer_get_layer(s_bt_layer));
  bluetooth_connection_service_subscribe(bt_handler);
  mark_dirty_info_layer();
}

static void deinit_info_layer(){
  bluetooth_connection_service_unsubscribe();
  text_layer_destroy(s_bt_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed){
  update_current_time();
  mark_dirty_time_layer();
  mark_dirty_tick_layer();
  hands_update_time_changed();
}

static void config_updated_callback(){
  Layer * root_layer = window_get_root_layer(s_main_window);
  layer_mark_dirty(root_layer);
  mark_dirty_time_layer();
  mark_dirty_tick_layer();
  hands_update_rainbow_mode_config_changed();
  hands_update_minute_hand_config_changed();
  hands_update_hour_hand_config_changed();
  window_set_background_color(s_main_window, config_get_background_color());
}

static void main_window_load(Window *window) {
  update_current_time();
  window_set_background_color(window, config_get_background_color());
  Layer * root_layer = window_get_root_layer(window);
  init_font();
  init_info_layer(root_layer);
  init_times(root_layer);
  init_tick_layer(root_layer);
  init_hands(root_layer);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void main_window_unload(Window *window) {
  deinit_hands();
  deinit_times();
  deinit_tick_layer();
  deinit_info_layer();
  deinit_font();
}

static void init() {
  init_config(config_updated_callback);
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
}

static void deinit() {
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
