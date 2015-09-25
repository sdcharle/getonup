#include <pebble.h>

static Window *s_countdown_window, *s_wakeup_window;
static TextLayer *s_error_text_layer, *s_tea_text_layer, *s_countdown_text_layer, *s_cancel_text_layer;

static BitmapLayer *s_getonup_bitmap_layer;
static GBitmap *s_getonup_bitmap;// ensure desTROYED!

static WakeupId s_wakeup_id = -1;
static time_t s_wakeup_timestamp = 0;
static char s_countdown_text[32];
static AppTimer *countdown_timer_handler;
static int WAKEUP_SECONDS = 10; //3600; // seconds between wakeups
// all gotta be static ferreal?
static int current_frame = 0; 
static int wakeup_live = 0; // is a wakeup active?
static uint32_t stand_pix[] = {
    RESOURCE_ID_SIT_PICTURE,
    RESOURCE_ID_STAND_PICTURE
};

static void bitmap_update() {
  if (s_getonup_bitmap) {
    gbitmap_destroy(s_getonup_bitmap);
  }
  s_getonup_bitmap = gbitmap_create_with_resource(stand_pix[current_frame%2]);
  bitmap_layer_set_bitmap(s_getonup_bitmap_layer, s_getonup_bitmap);  
}

enum {
  PERSIST_WAKEUP // Persistent storage key for wakeup_id
};

int wakeup_scheduled() {
  if (s_wakeup_id == -1) {
    return 0;
  } else {
    return 1;
  }
}

void set_cancel_text() {
  if (!wakeup_scheduled()) {
    text_layer_set_text(s_cancel_text_layer, "ON");          
  } else {
    text_layer_set_text(s_cancel_text_layer, "OFF");
  }
  
  if (!wakeup_scheduled()) {
    text_layer_set_text(s_tea_text_layer, "Alerts disabled."); 
    text_layer_set_text(s_countdown_text_layer, "");
  } else {
    text_layer_set_text(s_tea_text_layer, "Time 'til wakeup:");
    text_layer_set_text(s_countdown_text_layer, s_countdown_text);
  }
}

static void set_countdown_text() {
  int hrs, min, sec;
  if (s_wakeup_timestamp == 0) {
    // get the wakeup timestamp for showing a countdown
    wakeup_query(s_wakeup_id, &s_wakeup_timestamp);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "LOADED WACK-ASS TIME STAMP"); //" %d",s_wakeup_timestamp);
  }
  int countdown = s_wakeup_timestamp - time(NULL);
  
  hrs = countdown/3600;
  sec = countdown - hrs*3600;
  min = sec/60;
  sec = sec - min*60;    
  snprintf(s_countdown_text, sizeof(s_countdown_text), "%02d:%02d:%02d", hrs, min, sec);  
}

// do we have too many already?
static void timer_handler(void *data) {
  set_countdown_text();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "countdown tick."); 
  layer_mark_dirty(text_layer_get_layer(s_countdown_text_layer));
  if (wakeup_scheduled() && !wakeup_live)
    countdown_timer_handler = app_timer_register(1000, timer_handler, data);
}

static void set_next_wakeup() {  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "schedule wakeup");
  time_t wakeup_time = time(NULL) + WAKEUP_SECONDS;
  s_wakeup_id = wakeup_schedule(wakeup_time, 1, true); // 1?!?!?
  
  // get the new timestamp here
  wakeup_query(s_wakeup_id, &s_wakeup_timestamp);
  set_countdown_text();
  
  // If we couldn't schedule the wakeup event, display error_text overlay 
  if (s_wakeup_id <= 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "show error, could not sched.");
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
    return;
  }
  // Store the handle so we can cancel if necessary, or look it up next launch
  persist_write_int(PERSIST_WAKEUP, s_wakeup_id);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "register your timer");
  // do we want to do below or no?
  //if (!wakeup_live)
  //countdown_timer_handler = app_timer_register(1000, timer_handler, NULL); 
  // actually not a good idea.?
}

static void getonup_handler(void *data) {
  current_frame = current_frame + 1;
  bitmap_update();
  layer_mark_dirty(bitmap_layer_get_layer(s_getonup_bitmap_layer));
  app_timer_register(500, getonup_handler, data);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "next frame");  
}

static void countdown_back_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop_all(true); // That's pretty much it tho!
}

//  Toggle the current wakeup event on the countdown screen (should probly change name!)
static void countdown_cancel_handler(ClickRecognizerRef recognizer, void *context) {
  // actually toggle whether it's happening 
  if (wakeup_scheduled()) {  
      wakeup_cancel(s_wakeup_id);
      s_wakeup_id = -1;
      persist_delete(PERSIST_WAKEUP);  
      APP_LOG(APP_LOG_LEVEL_DEBUG, "cancel countdown."); 
      //app_timer_cancel(countdown_timer_handler);
  } else {
      set_next_wakeup();
      app_timer_register(0, timer_handler, NULL);
  }
  set_cancel_text();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Mark layers dirty after toggling.");  
  layer_mark_dirty(text_layer_get_layer(s_countdown_text_layer));
  layer_mark_dirty(text_layer_get_layer(s_tea_text_layer));     
  layer_mark_dirty(text_layer_get_layer(s_cancel_text_layer));
}

static void countdown_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, countdown_back_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, countdown_cancel_handler);
}

static void countdown_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "load countdown win."); 
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_click_config_provider(window, countdown_click_config_provider);
  
  s_tea_text_layer = text_layer_create(GRect(0, 32, bounds.size.w, 40));
  text_layer_set_font(s_tea_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));   
  text_layer_set_text_alignment(s_tea_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_tea_text_layer));

  s_countdown_text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 40));
  text_layer_set_font(s_countdown_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD)); 
  text_layer_set_text_alignment(s_countdown_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_countdown_text_layer));

  // Place ON or OFF next to the bottom button to toggle wakeup timer
  s_cancel_text_layer = text_layer_create(GRect(90, 116, 48, 28));
  set_cancel_text();
  text_layer_set_font(s_cancel_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_cancel_text_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_cancel_text_layer));

  s_wakeup_timestamp = 0;
  if (wakeup_scheduled() && !wakeup_live)
    countdown_timer_handler = app_timer_register(0, timer_handler, NULL);
}

static void countdown_window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "unload countdown win."); 
  text_layer_destroy(s_countdown_text_layer);
  text_layer_destroy(s_cancel_text_layer);
  text_layer_destroy(s_tea_text_layer);
}

static void wakeup_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Exit app after user is done w/ the alert
  APP_LOG(APP_LOG_LEVEL_DEBUG, "pop all from stack");
   set_next_wakeup();
  //if (s_getonup_bitmap)
  //  gbitmap_destroy(s_getonup_bitmap);
  //window_stack_pop(true);
  window_stack_pop_all(true);  
  // below clears the screen altogether
  //layer_set_hidden(bitmap_layer_get_layer(s_getonup_bitmap_layer), true);
  // verify pop all---really pops all
}

static void wakeup_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, wakeup_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, wakeup_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, wakeup_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, wakeup_click_handler);
}

static void wakeup_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "loading wakeup window");
  // crashes?
  Layer *window_layer = window_get_root_layer(window);

  wakeup_live = 1;
  window_set_click_config_provider(window, wakeup_click_config_provider);

  GRect bounds = layer_get_bounds(window_layer);
  s_getonup_bitmap_layer = bitmap_layer_create(bounds);
  s_getonup_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SIT_PICTURE);
  bitmap_layer_set_bitmap(s_getonup_bitmap_layer, s_getonup_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_getonup_bitmap_layer)); // just popping on top of what's there already
  
  bitmap_update();
  
  app_timer_register(600, getonup_handler, NULL);
}

static void wakeup_window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "unload wakeup win.");
  if (s_getonup_bitmap)
    gbitmap_destroy(s_getonup_bitmap);
  if (s_getonup_bitmap_layer)
    bitmap_layer_destroy(s_getonup_bitmap_layer); // this caused the Cinema dealy to crash!
}

static void wakeup_handler(WakeupId id, int32_t reason) {
  //Delete persistent storage value
  APP_LOG(APP_LOG_LEVEL_DEBUG, "handling wakeup");
  persist_delete(PERSIST_WAKEUP);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "deleted persisted");
  current_frame = 0;
  if (!wakeup_live) {
    window_stack_push(s_wakeup_window, false);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "pushed wakeup win on the stack");
  }
  vibes_double_pulse();
  //set_next_wakeup(); NOT YET!
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Set a wakeup after id %d reason %d",(int)id,(int)reason);
}

static void init(void) {
  bool wakeup_scheduled = false;
  wakeup_live = 0;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "LAUNCHED");
  // Check if we have already scheduled a wakeup event
  // so we can transition to the countdown window
  if (persist_exists(PERSIST_WAKEUP)) {
    s_wakeup_id = persist_read_int(PERSIST_WAKEUP);
    // query if event is still valid, otherwise delete
    if (wakeup_query(s_wakeup_id, NULL)) {
      wakeup_scheduled = true;
    } else {
      persist_delete(PERSIST_WAKEUP);
      s_wakeup_id = -1;
    }
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "ALREADY READ PERSIST SHIT!");

  s_countdown_window = window_create();
  window_set_window_handlers(s_countdown_window, (WindowHandlers){
    .load = countdown_window_load,
    .unload = countdown_window_unload,
  });
  APP_LOG(APP_LOG_LEVEL_DEBUG, "countdown created");
  s_wakeup_window = window_create();
  window_set_window_handlers(s_wakeup_window, (WindowHandlers){
    .load = wakeup_window_load,
    .unload = wakeup_window_unload,
  });
  
  window_stack_push(s_countdown_window, false);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "wakeup created");
  // Check to see if we were launched by a wakeup event
  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    // If woken by wakeup event, show the 'animation'
    WakeupId id = 0;
    int32_t reason = 0;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "woke by wakeup event");
    if (wakeup_get_launch_event(&id, &reason)) {
      wakeup_handler(id, reason);
    }
    // note I had pushed countdown win above!
  } else if (wakeup_scheduled) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "woke and wakeup scheduled");
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Woke and nothing scheduled");
  }
  // subscribe to wakeup service to get wakeup events while app is running
  wakeup_service_subscribe(wakeup_handler);
}

static void deinit(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit."); 
  window_destroy(s_countdown_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}