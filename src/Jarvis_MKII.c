#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "globals.h"


//0xFF, 0x81, 0x6C, 0xB5, 0xB4, 0xB8, 0x46, 0x85, 0x9D, 0x03, 0x69, 0x22, 0xDC, 0x2D, 0x27, 0x09
#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }
PBL_APP_INFO(MY_UUID,
             "Jarvis MK II", "Denics Tondus",
             1, 0, /* App version */
             RESOURCE_ID_APP_ICON,
             APP_INFO_STANDARD_APP);


Layer background_layer;

#define STRING_LENGTH 255
#define NUM_WEATHER_IMAGES	8

typedef enum {INFO_LAYER, CALENDAR_LAYER, MUSIC_LAYER, NUM_LAYERS} AnimatedLayers;


AppMessageResult sm_message_out_get(DictionaryIterator **iter_out);
void remplaceSpace(char *string);
void reset_sequence_number();
char* int_to_str(int num, char *outbuf);
void sendCommand(int key);
void sendCommandInt(int key, int param);
void rcv(DictionaryIterator *received, void *context);
void dropped(void *context, AppMessageResult reason);
void select_up_handler(ClickRecognizerRef recognizer, Window *window);
void select_down_handler(ClickRecognizerRef recognizer, Window *window);
void up_single_click_handler(ClickRecognizerRef recognizer, Window *window);
void down_single_click_handler(ClickRecognizerRef recognizer, Window *window);
void config_provider(ClickConfig **config, Window *window);
void battery_layer_update_callback(Layer *me, GContext* ctx);
void handle_status_appear(Window *window);
void handle_status_disappear(Window *window);
void handle_init(AppContextRef ctx);
void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t);
void handle_deinit(AppContextRef ctx);
void reset();

AppContextRef g_app_context;


static Window window;
static PropertyAnimation ani_out, ani_in;

static Layer animated_layer[NUM_LAYERS], weather_layer;
static Layer battery_layer, line_layer, text_battery_info_layer, text_weather_info_layer;

static TextLayer text_date_day_layer, text_date_month_layer, text_time_layer, text_write_power_layer;

static TextLayer text_weather_cond_layer, text_weather_temp_layer, text_battery_layer;
static TextLayer calendar_date_layer, calendar_text_layer;
static TextLayer music_artist_layer, music_song_layer;

static BitmapLayer background_image; //, weather_image, battery_image_layer;

static int active_layer;

static char string_buffer[STRING_LENGTH];

static char weather_cond_str[STRING_LENGTH], weather_temp_str[5];
static int batteryPercent; //weather_img,

static char calendar_date_str[STRING_LENGTH], calendar_text_str[STRING_LENGTH];
static char music_artist_str[STRING_LENGTH], music_title_str[STRING_LENGTH];


HeapBitmap bg_image, battery_image;
HeapBitmap weather_status_imgs[NUM_WEATHER_IMAGES];

static AppTimerHandle timerUpdateCalendar = 0;
static AppTimerHandle timerUpdateWeather = 0;
static AppTimerHandle timerUpdateMusic = 0;


/* DELETE WEATHER IMAGE
const int WEATHER_IMG_IDS[] = {
    RESOURCE_ID_IMAGE_SUN,
    RESOURCE_ID_IMAGE_RAIN,
    RESOURCE_ID_IMAGE_CLOUD,
    RESOURCE_ID_IMAGE_SUN_CLOUD,
    RESOURCE_ID_IMAGE_FOG,
    RESOURCE_ID_IMAGE_WIND,
    RESOURCE_ID_IMAGE_SNOW,
    RESOURCE_ID_IMAGE_THUNDER
};
*/



static uint32_t s_sequence_number = 0xFFFFFFFE;

AppMessageResult sm_message_out_get(DictionaryIterator **iter_out) {
    AppMessageResult result = app_message_out_get(iter_out);
    if(result != APP_MSG_OK) return result;
    dict_write_int32(*iter_out, SM_SEQUENCE_NUMBER_KEY, ++s_sequence_number);
    if(s_sequence_number == 0xFFFFFFFF) {
        s_sequence_number = 1;
    }
    return APP_MSG_OK;
}

void reset_sequence_number() {
    DictionaryIterator *iter = NULL;
    app_message_out_get(&iter);
    if(!iter) return;
    dict_write_int32(iter, SM_SEQUENCE_NUMBER_KEY, 0xFFFFFFFF);
    app_message_out_send();
    app_message_out_release();
}


char* int_to_str(int num, char *outbuf) {
	int digit, i=0, j=0;
	char buf[STRING_LENGTH];
	bool negative=false;
	
	if (num < 0) {
		negative = true;
		num = -1 * num;
	}
	
	for (i=0; i<STRING_LENGTH; i++) {
		digit = num % 10;
		if ((num==0) && (i>0))
			break;
		else
			buf[i] = '0' + digit;
        
		num/=10;
	}
	
	if (negative)
		buf[i++] = '-';
	
	buf[i--] = '\0';
	
	
	while (i>=0) {
		outbuf[j++] = buf[i--];
	}
    
	outbuf[j++] = ' '; // DELETE % SYMBOL
	//outbuf[j++] = '%';
	outbuf[j] = '\0';
	
	return outbuf;
}


void sendCommand(int key) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, -1);
	app_message_out_send();
	app_message_out_release();
}


void sendCommandInt(int key, int param) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, param);
	app_message_out_send();
	app_message_out_release();
}

void replaceSpace(char *string) {
    int x = strlen(string);
    int i;
    for(i = 0; i < x; i++)
    {
        if(*(string + i) == ' ')
            *(string + i) = '\n';
    }
}


void rcv(DictionaryIterator *received, void *context) {
	// Got a message callback
	Tuple *t;
    
    
	t=dict_find(received, SM_WEATHER_COND_KEY);
	if (t!=NULL) {
		memcpy(weather_cond_str, t->value->cstring, strlen(t->value->cstring));
        weather_cond_str[strlen(t->value->cstring)] = '\0';
        replaceSpace(weather_cond_str);
		text_layer_set_text(&text_weather_cond_layer, weather_cond_str);
	}
    
	t=dict_find(received, SM_WEATHER_TEMP_KEY);
	if (t!=NULL) {
		memcpy(weather_temp_str, t->value->cstring, strlen(t->value->cstring));
        weather_temp_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&text_weather_temp_layer, weather_temp_str);
		
		layer_set_hidden(&text_weather_cond_layer.layer, false); //true
		layer_set_hidden(&text_weather_temp_layer.layer, false);
        
	}
    /*
	t=dict_find(received, SM_WEATHER_ICON_KEY);
	if (t!=NULL) {
		bitmap_layer_set_bitmap(&weather_image, &weather_status_imgs[t->value->uint8].bmp);
	}
    */
	t=dict_find(received, SM_COUNT_BATTERY_KEY);
	if (t!=NULL) {
		batteryPercent = t->value->uint8;
		layer_mark_dirty(&battery_layer);
		text_layer_set_text(&text_battery_layer, int_to_str(batteryPercent, string_buffer) );
	}
    
	t=dict_find(received, SM_STATUS_CAL_TIME_KEY);
    if (t!=NULL) {
        memcpy(calendar_date_str, t->value->cstring, strlen(t->value->cstring));
        calendar_date_str[strlen(t->value->cstring)] = '\0';
        
        char str_month[2], str_day[2];
        
        memcpy(str_month, &calendar_date_str[0], 2);
        memcpy(str_day, &calendar_date_str[3], 2);
        
        memcpy(&calendar_date_str[0], str_day, 2);
        memcpy(&calendar_date_str[3], str_month, 2);
        
        text_layer_set_text(&calendar_date_layer, calendar_date_str);
    }
    
	t=dict_find(received, SM_STATUS_CAL_TEXT_KEY);
	if (t!=NULL) {
		memcpy(calendar_text_str, t->value->cstring, strlen(t->value->cstring));
        calendar_text_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&calendar_text_layer, calendar_text_str);
	}
    
    
	t=dict_find(received, SM_STATUS_MUS_ARTIST_KEY);
	if (t!=NULL) {
		memcpy(music_artist_str, t->value->cstring, strlen(t->value->cstring));
        music_artist_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&music_artist_layer, music_artist_str);
	}
    
	t=dict_find(received, SM_STATUS_MUS_TITLE_KEY);
	if (t!=NULL) {
		memcpy(music_title_str, t->value->cstring, strlen(t->value->cstring));
        music_title_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&music_song_layer, music_title_str);
	}
    
	t=dict_find(received, SM_STATUS_UPD_WEATHER_KEY);
	if (t!=NULL) {
		int interval = t->value->int32 * 1000;
        
		app_timer_cancel_event(g_app_context, timerUpdateWeather);
		timerUpdateWeather = app_timer_send_event(g_app_context, interval /* milliseconds */, 1);
	}
    
	t=dict_find(received, SM_STATUS_UPD_CAL_KEY);
	if (t!=NULL) {
		int interval = t->value->int32 * 1000;
        
		app_timer_cancel_event(g_app_context, timerUpdateCalendar);
		timerUpdateCalendar = app_timer_send_event(g_app_context, interval /* milliseconds */, 2);
	}
    
	t=dict_find(received, SM_SONG_LENGTH_KEY);
	if (t!=NULL) {
		int interval = t->value->int32 * 1000;
        
		app_timer_cancel_event(g_app_context, timerUpdateMusic);
		timerUpdateMusic = app_timer_send_event(g_app_context, interval /* milliseconds */, 3);
	}
    
}

void dropped(void *context, AppMessageResult reason){
	// DO SOMETHING WITH THE DROPPED REASON / DISPLAY AN ERROR / RESEND
}



void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    
    
}


void select_up_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    
	//revert to showing the temperature
	//layer_set_hidden(&text_weather_temp_layer.layer, false);
	//layer_set_hidden(&text_weather_cond_layer.layer, true);
    
}


void select_down_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    
	//show the weather condition instead of temperature while center button is pressed
	//layer_set_hidden(&text_weather_temp_layer.layer, true);
	//layer_set_hidden(&text_weather_cond_layer.layer, false);
    
}


void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    
	reset();
	
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
    
    
}


void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
	
	//on a press of the bottom button, scroll in the next layer
    
	property_animation_init_layer_frame(&ani_out, &animated_layer[active_layer], &GRect(0, 95, 144, 73), &GRect(-144, 95, 144, 73));
	animation_schedule(&(ani_out.animation));
    
    
	active_layer = (active_layer + 1) % (NUM_LAYERS);
    
    
	property_animation_init_layer_frame(&ani_in, &animated_layer[active_layer], &GRect(144, 95, 144, 73), &GRect(0, 95, 144, 73));
	animation_schedule(&(ani_in.animation));
	
}


void config_provider(ClickConfig **config, Window *window) {
    (void)window;
    
    
    //  config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
    config[BUTTON_ID_SELECT]->raw.up_handler = (ClickHandler) select_up_handler;
    config[BUTTON_ID_SELECT]->raw.down_handler = (ClickHandler) select_down_handler;
    
    //  config[BUTTON_ID_SELECT]->long_click.handler = (ClickHandler) select_long_click_handler;
    //  config[BUTTON_ID_SELECT]->long_click.release_handler = (ClickHandler) select_long_release_handler;
    
    
    config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
    config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;
    //  config[BUTTON_ID_UP]->long_click.handler = (ClickHandler) up_long_click_handler;
    //  config[BUTTON_ID_UP]->long_click.release_handler = (ClickHandler) up_long_release_handler;
    
    config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
    config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
    //  config[BUTTON_ID_DOWN]->long_click.handler = (ClickHandler) down_long_click_handler;
    //  config[BUTTON_ID_DOWN]->long_click.release_handler = (ClickHandler) down_long_release_handler;
    
}

/* DELETE BATTERY FILLING FUNCTION
void battery_layer_update_callback(Layer *me, GContext* ctx) {
	
	//draw the remaining battery percentage
	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_fill_color(ctx, GColorWhite);
    
	graphics_fill_rect(ctx, GRect(2+16-(int)((batteryPercent/100.0)*16.0), 2, (int)((batteryPercent/100.0)*16.0), 8), 0, GCornerNone);
	
}
*/

void handle_status_appear(Window *window)
{
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
}

void handle_status_disappear(Window *window)
{
	sendCommandInt(SM_SCREEN_EXIT_KEY, STATUS_SCREEN_APP);
	
	app_timer_cancel_event(g_app_context, timerUpdateCalendar);
	app_timer_cancel_event(g_app_context, timerUpdateMusic);
	app_timer_cancel_event(g_app_context, timerUpdateWeather);
	
}

void reset() {
	
	//layer_set_hidden(&text_weather_temp_layer.layer, true);
	//layer_set_hidden(&text_weather_cond_layer.layer, false);
	text_layer_set_text(&text_weather_cond_layer, "Updating..."); 	
	
}


// DRAW BACKGROUND LINES
void background_layer_update_callback(Layer *me, GContext* ctx) {

graphics_context_set_fill_color(ctx, GColorWhite); //Set fill color

graphics_draw_circle(ctx, GPoint(18, 18), 32); // External circle up left
graphics_draw_circle(ctx, GPoint(18, 18), 28); // Internal circle up left
    
/*
    graphics_fill_circle(ctx, GPoint(133, 160), 22); // circle bottom right white
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, GPoint(133, 160), 20); // circle bottom right black
*/

// Horizontal lines
graphics_draw_line(ctx, GPoint(46, 1), GPoint(138, 1)); //1
graphics_draw_line(ctx, GPoint(48, 31), GPoint(138, 31)); //2
graphics_draw_line(ctx, GPoint(56, 36), GPoint(142, 36)); //3
graphics_draw_line(ctx, GPoint(138, 6), GPoint(142, 6)); //SMALL


    
// Diagonal lines
//


// Vertical lines
graphics_draw_line(ctx, GPoint(138, 1), GPoint(138, 31)); //CADRE LEFT
graphics_draw_line(ctx, GPoint(142, 6), GPoint(142, 36)); //CADRE RIGHT 
graphics_draw_line(ctx, GPoint(125, 31), GPoint(125, 120)); //DECO LEFT
graphics_draw_line(ctx, GPoint(130, 31), GPoint(130, 100)); //DECO RIGHT


    
// Carrés déco
graphics_fill_rect(ctx, GRect(124, 123, 3, 3), 0, GCornerNone); //LEFT
graphics_fill_rect(ctx, GRect(129, 103, 3, 3), 0, GCornerNone); //RIGHT
    
// Rectangle déco
graphics_fill_rect(ctx, GRect(134, 50, 3, 45), 0, GCornerNone); //WHITE
graphics_context_set_fill_color(ctx, GColorBlack); //Set fill color
graphics_fill_rect(ctx, GRect(136, 55, 3, 30), 0, GCornerNone); //BLACK



}


void line_layer_update_callback(Layer *me, GContext* ctx) {
    
    
    
    graphics_context_set_fill_color(ctx, GColorWhite); //Set fill color
    
    
    graphics_draw_circle(ctx, GPoint(133, 64), 20); // circle bottom right
    graphics_draw_circle(ctx, GPoint(21, 56), 31); //circle bottom left grand
    graphics_draw_circle(ctx, GPoint(21, 56), 27); //circle bottom left petit

    graphics_draw_line(ctx, GPoint(70, 44), GPoint(114, 44));// UNDER WEATHER COND
    graphics_draw_line(ctx, GPoint(2, 14), GPoint(37, 14));// UNDER POWER WORD
    
    graphics_draw_line(ctx, GPoint(114, 44), GPoint(120, 48));// UNDER WEATHER COND
    
    graphics_draw_line(ctx, GPoint(2, 2), GPoint(2, 14)); //LEFT POWER WORD
    //graphics_draw_line(ctx, GPoint(37, 9), GPoint(37, 14)); //RIGHT POWER WORD
    graphics_draw_line(ctx, GPoint(20, 14), GPoint(20, 25)); //UNDER POWER WORD

}
void handle_init(AppContextRef ctx) {
    (void)ctx;

    g_app_context = ctx;
    
	window_init(&window, "Window Name");
	window_set_window_handlers(&window, (WindowHandlers) {
	    .appear = (WindowHandler)handle_status_appear,
	    .disappear = (WindowHandler)handle_status_disappear
	});
    
	window_stack_push(&window, true /* Animated */);
	window_set_fullscreen(&window, true);
    
	resource_init_current_app(&APP_RESOURCES);
    
    /* REMOVE WEATHER IMAGE
	//init weather images
	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
		heap_bitmap_init(&weather_status_imgs[i], WEATHER_IMG_IDS[i]);
	}
	*/
	heap_bitmap_init(&bg_image, RESOURCE_ID_IMAGE_BACKGROUND);
    
    
	//init background image
	bitmap_layer_init(&background_image, GRect(0, 0, 144, 168));
	layer_add_child(&window.layer, &background_image.layer);
	bitmap_layer_set_bitmap(&background_image, &bg_image.bmp);
    
    //init layer for date day
	text_layer_init(&text_date_day_layer, window.layer.frame);
	text_layer_set_text_alignment(&text_date_day_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_date_day_layer, GColorWhite);
	text_layer_set_background_color(&text_date_day_layer, GColorClear);
	layer_set_frame(&text_date_day_layer.layer, GRect(-53, 8, 144, 30));
	text_layer_set_font(&text_date_day_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IRONMAN_30)));
	layer_add_child(&window.layer, &text_date_day_layer.layer);
    
    //init layer for date month
	text_layer_init(&text_date_month_layer, window.layer.frame);
	text_layer_set_text_alignment(&text_date_month_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_date_month_layer, GColorWhite);
	text_layer_set_background_color(&text_date_month_layer, GColorClear);
	layer_set_frame(&text_date_month_layer.layer, GRect(-53, 0, 144, 30));
	text_layer_set_font(&text_date_month_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IRONMAN_12)));
	layer_add_child(&window.layer, &text_date_month_layer.layer);
    
    //init layer for time
	text_layer_init(&text_time_layer, window.layer.frame);
	text_layer_set_text_alignment(&text_time_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_time_layer, GColorWhite);
	text_layer_set_background_color(&text_time_layer, GColorClear);
	layer_set_frame(&text_time_layer.layer, GRect(35, -2, 144, 50));
	text_layer_set_font(&text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IRONMAN_30)));
	layer_add_child(&window.layer, &text_time_layer.layer);
    
    
    layer_init(&animated_layer[INFO_LAYER], GRect(0, 95, 144, 73));
    layer_add_child(&window.layer, &animated_layer[INFO_LAYER]);
    
    text_layer_init(&text_battery_layer, GRect(-8, 35, 60, 73));
	text_layer_set_text_alignment(&text_battery_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_battery_layer, GColorWhite);
	text_layer_set_background_color(&text_battery_layer, GColorClear);
    text_layer_set_font(&text_battery_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IRONMAN_30)));
    
	layer_add_child(&animated_layer[INFO_LAYER], &text_battery_layer.layer);
	text_layer_set_text(&text_battery_layer, "-");
    
    
    text_layer_init(&text_weather_cond_layer, GRect(70, 25, 70, 70)); // GRect(5, 2, 47, 40)
	text_layer_set_text_alignment(&text_weather_cond_layer, GTextAlignmentLeft);
	text_layer_set_text_color(&text_weather_cond_layer, GColorWhite);
	text_layer_set_background_color(&text_weather_cond_layer, GColorClear);
	text_layer_set_font(&text_weather_cond_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MICRO_15)));
	layer_add_child(&animated_layer[INFO_LAYER], &text_weather_cond_layer.layer);
    
	layer_set_hidden(&text_weather_cond_layer.layer, false);
	text_layer_set_text(&text_weather_cond_layer, "Updating...");
    
    
    
    text_layer_init(&text_weather_temp_layer, GRect(119, 48, 25, 25)); // GRect(98, 4, 47, 40)
	text_layer_set_text_alignment(&text_weather_temp_layer, GTextAlignmentRight);
	text_layer_set_text_color(&text_weather_temp_layer, GColorWhite);
	text_layer_set_background_color(&text_weather_temp_layer, GColorClear);
	text_layer_set_font(&text_weather_temp_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IRONMAN_21)));
	layer_add_child(&animated_layer[INFO_LAYER], &text_weather_temp_layer.layer);
	text_layer_set_text(&text_weather_temp_layer, "-°");
    
    
    
    layer_init(&line_layer, GRect(0, 0, 144, 73));
    line_layer.update_proc = &line_layer_update_callback;
    layer_add_child(&animated_layer[INFO_LAYER], &line_layer);
    
    text_layer_init(&text_write_power_layer, GRect(6, -5, 30, 18)); // GRect(5, 2, 47, 40)
	text_layer_set_text_alignment(&text_write_power_layer, GTextAlignmentLeft);
	text_layer_set_text_color(&text_write_power_layer, GColorWhite);
	text_layer_set_background_color(&text_write_power_layer, GColorClear);
	text_layer_set_font(&text_write_power_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MICRO_15)));
	layer_add_child(&animated_layer[INFO_LAYER], &text_write_power_layer.layer);
	text_layer_set_text(&text_write_power_layer, "Power");

    
    
    
    
	layer_set_hidden(&text_weather_temp_layer.layer, true);
    
    
    layer_init(&animated_layer[CALENDAR_LAYER], GRect(144, 95, 144, 73));
    layer_add_child(&window.layer, &animated_layer[CALENDAR_LAYER]);
    
    text_layer_init(&calendar_date_layer, GRect(6, 28, 132, 21));
    text_layer_set_text_alignment(&calendar_date_layer, GTextAlignmentLeft);
    text_layer_set_text_color(&calendar_date_layer, GColorWhite);
    text_layer_set_background_color(&calendar_date_layer, GColorClear);
    text_layer_set_font(&calendar_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    layer_add_child(&animated_layer[CALENDAR_LAYER], &calendar_date_layer.layer);
    text_layer_set_text(&calendar_date_layer, "No Upcoming");
    
    
    text_layer_init(&calendar_text_layer, GRect(6, 43, 132, 28));
    text_layer_set_text_alignment(&calendar_text_layer, GTextAlignmentLeft);
    text_layer_set_text_color(&calendar_text_layer, GColorWhite);
    text_layer_set_background_color(&calendar_text_layer, GColorClear);
    text_layer_set_font(&calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    layer_add_child(&animated_layer[CALENDAR_LAYER], &calendar_text_layer.layer);
    text_layer_set_text(&calendar_text_layer, "Appointment");
    
    
    
    //init music layer
    layer_init(&animated_layer[MUSIC_LAYER], GRect(288, 95, 144, 73));
    layer_add_child(&window.layer, &animated_layer[MUSIC_LAYER]);
    
    text_layer_init(&music_artist_layer, GRect(6, 28, 132, 21));
    text_layer_set_text_alignment(&music_artist_layer, GTextAlignmentLeft);
    text_layer_set_text_color(&music_artist_layer, GColorWhite);
    text_layer_set_background_color(&music_artist_layer, GColorClear);
    text_layer_set_font(&music_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    layer_add_child(&animated_layer[MUSIC_LAYER], &music_artist_layer.layer);
    text_layer_set_text(&music_artist_layer, "Artist");
    
    
    text_layer_init(&music_song_layer, GRect(6, 43, 132, 28));
    text_layer_set_text_alignment(&music_song_layer, GTextAlignmentLeft);
    text_layer_set_text_color(&music_song_layer, GColorWhite);
    text_layer_set_background_color(&music_song_layer, GColorClear);
    text_layer_set_font(&music_song_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    layer_add_child(&animated_layer[MUSIC_LAYER], &music_song_layer.layer);
    text_layer_set_text(&music_song_layer, "Title");


    
    
/*
______________________________________________________________________________________________________________________________________
	//init weather layer and add weather image, weather condition, temperature, and battery indicator
	layer_init(&weather_layer, GRect(0, 95, 144, 73));  //GRect(0, 78, 144, 45) tier inférieur
	layer_add_child(&window.layer, &weather_layer);
    
     DELETE EMPTY BATTERY ICON
	heap_bitmap_init(&battery_image, RESOURCE_ID_IMAGE_BATTERY);
    
	bitmap_layer_init(&battery_image_layer, GRect(107, 8, 23, 14));
	layer_add_child(&weather_layer, &battery_image_layer.layer);
	bitmap_layer_set_bitmap(&battery_image_layer, &battery_image.bmp);
 
    
	text_layer_init(&text_battery_layer, GRect(-3, 24, 60, 30));
	text_layer_set_text_alignment(&text_battery_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_battery_layer, GColorWhite);
	text_layer_set_background_color(&text_battery_layer, GColorClear);
    text_layer_set_font(&text_battery_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IRONMAN_21)));

	layer_add_child(&weather_layer, &text_battery_layer.layer);
	text_layer_set_text(&text_battery_layer, "-");
    
    //Write POWER text on the background
    
    text_layer_init(&text_write_power_layer, GRect(5, 90, 30, 18)); // GRect(5, 2, 47, 40)
	text_layer_set_text_alignment(&text_write_power_layer, GTextAlignmentLeft);
	text_layer_set_text_color(&text_write_power_layer, GColorWhite);
	text_layer_set_background_color(&text_write_power_layer, GColorClear);
	text_layer_set_font(&text_write_power_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MICRO_15)));
	layer_add_child(&window.layer, &text_write_power_layer.layer);
	text_layer_set_text(&text_write_power_layer, "Power");

    
     DELETE FILLING BATTERY ICON
	layer_init(&battery_layer, GRect(109, 9, 19, 11));
	battery_layer.update_proc = &battery_layer_update_callback;
	layer_add_child(&weather_layer, &battery_layer);
    
	batteryPercent = 100;
	layer_mark_dirty(&battery_layer);
 
    
	text_layer_init(&text_weather_cond_layer, GRect(70, 8, 144-70, 56-8)); // GRect(5, 2, 47, 40)
	text_layer_set_text_alignment(&text_weather_cond_layer, GTextAlignmentLeft);
	text_layer_set_text_color(&text_weather_cond_layer, GColorWhite);
	text_layer_set_background_color(&text_weather_cond_layer, GColorClear);
	text_layer_set_font(&text_weather_cond_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MICRO_15)));
	layer_add_child(&weather_layer, &text_weather_cond_layer.layer);
    
	layer_set_hidden(&text_weather_cond_layer.layer, false);
	text_layer_set_text(&text_weather_cond_layer, "Updating...");
	
 
	weather_img = 0;
    
	bitmap_layer_init(&weather_image, GRect(5, 2, 40, 40)); // GRect(52, 2, 40, 40)
	layer_add_child(&weather_layer, &weather_image.layer);
	bitmap_layer_set_bitmap(&weather_image, &weather_status_imgs[0].bmp);
 
    
    text_layer_init(&text_weather_temp_layer, GRect(144-47, 30, 48, 22)); // GRect(98, 4, 47, 40)
	text_layer_set_text_alignment(&text_weather_temp_layer, GTextAlignmentRight);
	text_layer_set_text_color(&text_weather_temp_layer, GColorWhite);
	text_layer_set_background_color(&text_weather_temp_layer, GColorClear);
	text_layer_set_font(&text_weather_temp_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IRONMAN_21)));
	layer_add_child(&weather_layer, &text_weather_temp_layer.layer);
	text_layer_set_text(&text_weather_temp_layer, "-°");
    
	layer_set_hidden(&text_weather_temp_layer.layer, true);
    
	
	//init layer for date
	text_layer_init(&text_date_layer, window.layer.frame);
	text_layer_set_text_alignment(&text_date_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_date_layer, GColorWhite);
	text_layer_set_background_color(&text_date_layer, GColorClear);
	layer_set_frame(&text_date_layer.layer, GRect(-53, 8, 144, 30));
	text_layer_set_font(&text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IRONMAN_30)));
	layer_add_child(&window.layer, &text_date_layer.layer);
    
    //init layer for time
	text_layer_init(&text_time_layer, window.layer.frame);
	text_layer_set_text_alignment(&text_time_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_time_layer, GColorWhite);
	text_layer_set_background_color(&text_time_layer, GColorClear);
	layer_set_frame(&text_time_layer.layer, GRect(35, -3, 144, 50));
	text_layer_set_font(&text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IRONMAN_30)));
	layer_add_child(&window.layer, &text_time_layer.layer);
    
     DELETE CALENDAR, MUSIC INFO AND INFO
	//init info layer
   
    layer_init(&animated_layer[INFO_LAYER], GRect(0, 124, 144, 45));
    layer_add_child(&windows.layer, &animated_layer[INFO_LAYER]);
     
     
     
     
    //init calendar layer
	layer_init(&animated_layer[CALENDAR_LAYER], GRect(0, 124, 144, 45));
	layer_add_child(&window.layer, &animated_layer[CALENDAR_LAYER]);
	
	text_layer_init(&calendar_date_layer, GRect(6, 0, 132, 21));
	text_layer_set_text_alignment(&calendar_date_layer, GTextAlignmentLeft);
	text_layer_set_text_color(&calendar_date_layer, GColorWhite);
	text_layer_set_background_color(&calendar_date_layer, GColorClear);
	text_layer_set_font(&calendar_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(&animated_layer[CALENDAR_LAYER], &calendar_date_layer.layer);
	text_layer_set_text(&calendar_date_layer, "No Upcoming");
    
    
	text_layer_init(&calendar_text_layer, GRect(6, 15, 132, 28));
	text_layer_set_text_alignment(&calendar_text_layer, GTextAlignmentLeft);
	text_layer_set_text_color(&calendar_text_layer, GColorWhite);
	text_layer_set_background_color(&calendar_text_layer, GColorClear);
	text_layer_set_font(&calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(&animated_layer[CALENDAR_LAYER], &calendar_text_layer.layer);
	text_layer_set_text(&calendar_text_layer, "Appointment");
	
	
	
	//init music layer
	layer_init(&animated_layer[MUSIC_LAYER], GRect(144, 124, 144, 45));
	layer_add_child(&window.layer, &animated_layer[MUSIC_LAYER]);
	
	text_layer_init(&music_artist_layer, GRect(6, 0, 132, 21));
	text_layer_set_text_alignment(&music_artist_layer, GTextAlignmentLeft);
	text_layer_set_text_color(&music_artist_layer, GColorWhite);
	text_layer_set_background_color(&music_artist_layer, GColorClear);
	text_layer_set_font(&music_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(&animated_layer[MUSIC_LAYER], &music_artist_layer.layer);
	text_layer_set_text(&music_artist_layer, "Artist");
    
    
	text_layer_init(&music_song_layer, GRect(6, 15, 132, 28));
	text_layer_set_text_alignment(&music_song_layer, GTextAlignmentLeft);
	text_layer_set_text_color(&music_song_layer, GColorWhite);
	text_layer_set_background_color(&music_song_layer, GColorClear);
	text_layer_set_font(&music_song_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(&animated_layer[MUSIC_LAYER], &music_song_layer.layer);
	text_layer_set_text(&music_song_layer, "Title");
	*/
    
    
    //init background layer MOI
    layer_init(&background_layer, GRect(0, 0, 144, 168));
    background_layer.update_proc = &background_layer_update_callback;
    layer_add_child(&window.layer, &background_layer);
	   
    
    window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);
    
	//active_layer = CALENDAR_LAYER;
    active_layer = INFO_LAYER;
    
	reset();

}


void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {
    /* Display the time */
    (void)ctx;
    
    static char time_text[] = "00:00";
    static char date_day_text[] = "00";
    static char date_month_text[] = "XXXX";
    
    
    char *time_format;
    
    //%a=jour abbr. %b=mois abbr.
    string_format_time(date_day_text, sizeof(date_day_text), "%e", t->tick_time);
    text_layer_set_text(&text_date_day_layer, date_day_text);
    
    string_format_time(date_month_text, sizeof(date_month_text), "%b", t->tick_time);
    text_layer_set_text(&text_date_month_layer, date_month_text);
    
    
    if (clock_is_24h_style()) {
        time_format = "%R";
    } else {
        time_format = "%I:%M";
    }
    
    string_format_time(time_text, sizeof(time_text), time_format, t->tick_time);
    
    if (!clock_is_24h_style() && (time_text[0] == '0')) {
        memmove(time_text, &time_text[1], sizeof(time_text) - 1);
    }
    
    text_layer_set_text(&text_time_layer, time_text);
    
}

void handle_deinit(AppContextRef ctx) {
    (void)ctx;
    
	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
	  	heap_bitmap_deinit(&weather_status_imgs[i]);
	}
    
  	heap_bitmap_deinit(&bg_image);
    
	
}


void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie) {
    (void)ctx;
    (void)handle;
    
    /* Request new data from the phone once the timers expire */
    
	if (cookie == 1) {
		sendCommand(SM_STATUS_UPD_WEATHER_KEY);
	}
    
	if (cookie == 2) {
		sendCommand(SM_STATUS_UPD_CAL_KEY);
	}
    
	if (cookie == 3) {
		sendCommand(SM_SONG_LENGTH_KEY);
	}
    
	
    
}

void pbl_main(void *params) {
    
    PebbleAppHandlers handlers = {
        .init_handler = &handle_init,
        .deinit_handler = &handle_deinit,
        .messaging_info = {
            .buffer_sizes = {
                .inbound = 124,
                .outbound = 256
            },
            .default_callbacks.callbacks = {
                .in_received = rcv,
                .in_dropped = dropped
            }
        },
        .tick_info = {
            .tick_handler = &handle_minute_tick,
            .tick_units = MINUTE_UNIT
        },
        .timer_handler = &handle_timer,
        
    };
    app_event_loop(params, &handlers);
}
