/*************************************************************
 *                     FIRMINIA 3.0                          *
 *  File: display_manager.c                                  *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

 #include "display_manager.h"
 #include <stdio.h>
 #include <unistd.h>
 #include <string.h>
 #include <sys/lock.h>
 #include <sys/param.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "esp_timer.h"
 #include "esp_lcd_panel_io.h"
 #include "esp_lcd_panel_vendor.h"
 #include "esp_lcd_panel_ops.h"
 #include "driver/gpio.h"
 #include "driver/spi_master.h"
 #include "esp_err.h"
 #include "esp_log.h"
 #include "lvgl.h"
 #include "esp_heap_caps.h"
 #include "esp_lcd_gc9a01.h"
 #include "device_config.h"

 static const char *TAG = "display";
 
 // Using SPI2 in the example
 #define LCD_HOST               SPI2_HOST
 #define LCD_PIXEL_CLOCK_HZ     (15 * 1000 * 1000)
 #define LCD_BK_LIGHT_ON_LEVEL  1
 #define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
 #define PIN_NUM_SCLK           7
 #define PIN_NUM_MOSI           9 // sda
 #define PIN_NUM_MISO           -1
 #define PIN_NUM_LCD_DC         4
 #define PIN_NUM_LCD_RST        3
 #define PIN_NUM_LCD_CS         6
 #define PIN_NUM_BK_LIGHT       2
 #define PIN_NUM_TOUCH_CS       -1
 
 // The pixel number in horizontal and vertical
 #define LCD_H_RES              240
 #define LCD_V_RES              240
 
 // Bit number used to represent command and parameter
 #define LCD_CMD_BITS           8
 #define LCD_PARAM_BITS         8
 
 // LVGL buffer configuration
 #define LVGL_DRAW_BUF_LINES    30
 #define LVGL_TICK_PERIOD_MS    2
 #define LVGL_TASK_MAX_DELAY_MS 500
 #define LVGL_TASK_MIN_DELAY_MS 1
 #define LVGL_TASK_STACK_SIZE   (8 * 1024)
 #define LVGL_TASK_PRIORITY     2
 
 // Font definitions
 extern const lv_font_t lv_font_montserrat_18;
 extern const lv_font_t lv_font_montserrat_28;
 extern const lv_font_t lv_font_montserrat_48;
 
 static lv_obj_t *number_label = NULL;
 static const lv_font_t *pending_font = &lv_font_montserrat_28;  // font di default

 // Mutex to protect LVGL calls
 static _lock_t lvgl_api_lock;
 
 // Global pointer to the LVGL label for displaying state text
 static lv_obj_t *state_label = NULL;

 // Global buffer for the new text to display
 static char new_text[256] = {0};
 
 // Global pointer for the animated arc
 static lv_obj_t *state_arc = NULL;
 #define ARC_LINE_WIDTH 5
 #define ARC_SPAN 45
 
 // Definition of colors for the arc
 #define ARC_COLOR_WARMING   lv_color_hex(0x800080)  // Viola
 #define ARC_COLOR_BLE       lv_color_hex(0x0000FF)  // Blu
 #define ARC_COLOR_WIFI      lv_color_hex(0xFFFF00)  // Giallo
 #define ARC_COLOR_API       lv_color_hex(0x00FF00)  // Verde
 
 // Forward declarations: all functions maintained without removing references or simplifications
 static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                     esp_lcd_panel_io_event_data_t *edata,
                                     void *user_ctx);
 static void lvgl_port_update_callback(lv_display_t *disp);
 static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
 static void increase_lvgl_tick(void *arg);
 static void lvgl_port_task(void *arg);
 
 //------------------------------------------------------------------------------
 // Callback to update the label opacity during fade in/out
 //------------------------------------------------------------------------------
 static void anim_opacity_cb(void *var, int32_t v)
 {
     // Use the label passed as var, avoiding direct access to 'state_label'
     if (!var) return;
     lv_obj_t *lbl = (lv_obj_t *)var;
     lv_obj_set_style_opa(lbl, v, 0);
 }
 
 //------------------------------------------------------------------------------
 // Callback for fade-in animation
 //------------------------------------------------------------------------------
 static void fade_in_anim_cb(void *var, int32_t v)
 {
     if (!var) return;
     lv_obj_t *lbl = (lv_obj_t *)var;
     lv_obj_set_style_opa(lbl, v, 0);
 }
 
 //------------------------------------------------------------------------------
 // Starts the fade-in animation (0->255)
 static void fade_in_anim_start(lv_obj_t *lbl)
 {
     if (!lbl) return;
 
     // Remove any previous animations on 'lbl'
     lv_anim_del(lbl, NULL);
 
     lv_anim_t a;
     lv_anim_init(&a);
     lv_anim_set_var(&a, lbl);
     lv_anim_set_exec_cb(&a, fade_in_anim_cb);
     lv_anim_set_values(&a, 0, 255);
     lv_anim_set_time(&a, 200);
     lv_anim_start(&a);
 }
 
 //------------------------------------------------------------------------------
 // Callback at the end of fade-out
 // Here we update the text, then start fade-in
 //------------------------------------------------------------------------------
static void fade_out_anim_ready_cb(lv_anim_t *a)
{
    if(!a || !a->var) return;
    lv_obj_t *lbl = (lv_obj_t*)a->var;

    /* 1. testo nuovo */
    lv_label_set_text(lbl, new_text);

    /* 2. font nuovo */
    lv_obj_set_style_text_font(lbl, pending_font, 0);

    /* 3. riallinea e fade-in */
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    fade_in_anim_start(lbl);
}
 
 //------------------------------------------------------------------------------
 // Starts the fade-out animation (255->0) with callback
 //------------------------------------------------------------------------------
 static void fade_out_anim_start(lv_obj_t *lbl)
 {
     if (!lbl) return;
 
     // Removes any ongoing animations on lbl
     lv_anim_del(lbl, NULL);
 
     lv_anim_t a_out;
     lv_anim_init(&a_out);
     lv_anim_set_var(&a_out, lbl);
     lv_anim_set_exec_cb(&a_out, anim_opacity_cb);
     lv_anim_set_values(&a_out, 255, 0);
     lv_anim_set_time(&a_out, 200);
     lv_anim_set_ready_cb(&a_out, fade_out_anim_ready_cb);
     lv_anim_start(&a_out);
 }
 
 //------------------------------------------------------------------------------
 // notify_lvgl_flush_ready
 //------------------------------------------------------------------------------
 static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                     esp_lcd_panel_io_event_data_t *edata,
                                     void *user_ctx)
 {
     lv_display_t *disp = (lv_display_t *)user_ctx;
     lv_display_flush_ready(disp);
     return false;
 }
 
 //------------------------------------------------------------------------------
 // lvgl_port_update_callback
 //------------------------------------------------------------------------------
 static void lvgl_port_update_callback(lv_display_t *disp)
 {
     esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
     lv_display_rotation_t rotation = lv_display_get_rotation(disp);
 
     switch (rotation) {
         case LV_DISPLAY_ROTATION_0:
             esp_lcd_panel_swap_xy(panel_handle, false);
             esp_lcd_panel_mirror(panel_handle, true, false);
             break;
         case LV_DISPLAY_ROTATION_90:
             esp_lcd_panel_swap_xy(panel_handle, true);
             esp_lcd_panel_mirror(panel_handle, true, true);
             break;
         case LV_DISPLAY_ROTATION_180:
             esp_lcd_panel_swap_xy(panel_handle, false);
             esp_lcd_panel_mirror(panel_handle, false, true);
             break;
         case LV_DISPLAY_ROTATION_270:
             esp_lcd_panel_swap_xy(panel_handle, true);
             esp_lcd_panel_mirror(panel_handle, false, false);
             break;
     }
 }
 
 //------------------------------------------------------------------------------
 // lvgl_flush_cb
 //------------------------------------------------------------------------------
 static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
 {
     lvgl_port_update_callback(disp);
     esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
     int offsetx1 = area->x1;
     int offsetx2 = area->x2;
     int offsety1 = area->y1;
     int offsety2 = area->y2;
     lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
     esp_err_t err = esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1,
                                               offsetx2 + 1, offsety2 + 1, px_map);
     if (err != ESP_OK) {
         ESP_LOGE(TAG, "panel_gc9a01_draw_bitmap failed: %d", err);
         vTaskDelay(pdMS_TO_TICKS(10));
     }
 }
 
 //------------------------------------------------------------------------------
 // increase_lvgl_tick
 //------------------------------------------------------------------------------
 static void increase_lvgl_tick(void *arg)
 {
     lv_tick_inc(LVGL_TICK_PERIOD_MS);
 }
 
 //------------------------------------------------------------------------------
 // lvgl_port_task
 //------------------------------------------------------------------------------
 static void lvgl_port_task(void *arg)
 {
     ESP_LOGI(TAG, "Starting LVGL task");
     while (1) {
         _lock_acquire(&lvgl_api_lock);
         uint32_t time_till_next_ms = lv_timer_handler();
         _lock_release(&lvgl_api_lock);
 
         if (time_till_next_ms < LVGL_TASK_MIN_DELAY_MS) {
             time_till_next_ms = LVGL_TASK_MIN_DELAY_MS;
         }
         vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
     }
 }
 
 //------------------------------------------------------------------------------
 // arc_anim_cb - Ruota l'arco
 //------------------------------------------------------------------------------
 static void arc_anim_cb(void *var, int32_t angle)
 {
     lv_obj_t *arc = (lv_obj_t *)var;
     if (!arc) return;
     int16_t start = angle % 360;
     lv_arc_set_angles(arc, start, start + ARC_SPAN);
 }
 
 //------------------------------------------------------------------------------
 // display_manager_init
 //------------------------------------------------------------------------------
 void display_manager_init(void)
 {
     ESP_LOGI(TAG, "Heap disponibile prima dell'inizializzazione display: %lu bytes",
              (unsigned long) esp_get_free_heap_size());
     _lock_init(&lvgl_api_lock);
 
     gpio_config_t bk_gpio_config = {
         .pin_bit_mask = (1ULL << PIN_NUM_BK_LIGHT),
         .mode = GPIO_MODE_OUTPUT,
     };
     ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
 
     ESP_LOGI(TAG, "Turn off LCD backlight");
     gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_OFF_LEVEL);
 
     ESP_LOGI(TAG, "Initialize SPI bus");
     spi_bus_config_t buscfg = {
         .sclk_io_num = PIN_NUM_SCLK,
         .mosi_io_num = PIN_NUM_MOSI,
         .miso_io_num = PIN_NUM_MISO,
         .quadwp_io_num = -1,
         .quadhd_io_num = -1,
         .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
     };
     ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
 
     ESP_LOGI(TAG, "Install panel IO");
     esp_lcd_panel_io_handle_t io_handle = NULL;
     esp_lcd_panel_io_spi_config_t io_config = {
         .dc_gpio_num = PIN_NUM_LCD_DC,
         .cs_gpio_num = PIN_NUM_LCD_CS,
         .pclk_hz = LCD_PIXEL_CLOCK_HZ,
         .lcd_cmd_bits = 8,
         .lcd_param_bits = 8,
         .spi_mode = 0,
         .trans_queue_depth = 10,
     };
     ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
         (esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
 
     esp_lcd_panel_handle_t panel_handle = NULL;
     esp_lcd_panel_dev_config_t panel_config = {
         .reset_gpio_num = PIN_NUM_LCD_RST,
         .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
         .bits_per_pixel = 16,
     };
     ESP_LOGI(TAG, "Install GC9A01 panel driver");
     ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
     ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
     ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
     ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
 
     ESP_LOGI(TAG, "Clear LCD with black color");
     const size_t lines_per_transfer = 20;
     uint16_t *black_buffer = heap_caps_calloc(LCD_H_RES * lines_per_transfer,
                                               sizeof(uint16_t), MALLOC_CAP_DMA);
     assert(black_buffer);
 
     for (int y = 0; y < LCD_V_RES; y += lines_per_transfer) {
         int lines = MIN(lines_per_transfer, LCD_V_RES - y);
         ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle,
             0, y,
             LCD_H_RES, y + lines,
             black_buffer));
     }
     heap_caps_free(black_buffer);
     vTaskDelay(pdMS_TO_TICKS(100));
 
     ESP_LOGI(TAG, "Turn on LCD backlight");
     gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL);
 
     ESP_LOGI(TAG, "Initialize LVGL library");
     lv_init();
 
     lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
     size_t draw_buffer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);
     void *buf1 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
     if (!buf1) {
         ESP_LOGE(TAG, "Allocazione PSRAM per draw buffer 1 fallita, uso malloc()");
         buf1 = malloc(draw_buffer_sz);
     }
     void *buf2 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
     if (!buf2) {
         ESP_LOGE(TAG, "Allocazione PSRAM per draw buffer 2 fallita, uso malloc()");
         buf2 = malloc(draw_buffer_sz);
     }
     assert(buf1 && buf2);
 
     lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz,
                            LV_DISPLAY_RENDER_MODE_PARTIAL);
     lv_display_set_user_data(display, panel_handle);
     lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
     lv_display_set_flush_cb(display, lvgl_flush_cb);
 
     ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
 
     ESP_LOGI(TAG, "Install LVGL tick timer");
     const esp_timer_create_args_t lvgl_tick_timer_args = {
         .callback = &increase_lvgl_tick,
         .name = "lvgl_tick"
     };
     esp_timer_handle_t lvgl_tick_timer = NULL;
     ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
     ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer,
                         LVGL_TICK_PERIOD_MS * 1000));
 
     ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
     const esp_lcd_panel_io_callbacks_t cbs = {
         .on_color_trans_done = notify_lvgl_flush_ready,
     };
     ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));
 
     ESP_LOGI(TAG, "Create LVGL task");
     xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL,
                 LVGL_TASK_PRIORITY, NULL);
 
     ESP_LOGI(TAG, "Heap disponibile dopo inizializzazione display: %lu bytes",
              (unsigned long) esp_get_free_heap_size());
 }
 
 //------------------------------------------------------------------------------
 // display_manager_update
 //------------------------------------------------------------------------------
 void display_manager_update(display_state_t state, int practices_count)
 {
     // Protect the entire function with a lock
     _lock_acquire(&lvgl_api_lock);
 
     // Update new_text according to the state
     switch (state) {
         case DISPLAY_STATE_WARMING_UP:
             strcpy(new_text, LV_SYMBOL_POWER "\nWarming\nup...\n\nv3.1.1");
             break;
         case DISPLAY_STATE_BLE_ADVERTISING:
             strcpy(new_text, LV_SYMBOL_BLUETOOTH "\nBT activated.\nWaiting for\nconfig...");
             break;
         case DISPLAY_STATE_CONFIG_UPDATED:
             strcpy(new_text, LV_SYMBOL_OK "\nConfiguration\nupdated!");
             break;
         case DISPLAY_STATE_WIFI_CONNECTING:
             strcpy(new_text, LV_SYMBOL_WIFI "\nConnecting\nto Wi-Fi...");
             break;
        case DISPLAY_STATE_CHECKING_API: {
            char short_user[16];
            strncpy(short_user, askmesign_user, 15);
            short_user[15] = '\0';
            
            pending_font = &lv_font_montserrat_18;

            snprintf(new_text, sizeof(new_text),
                    LV_SYMBOL_REFRESH "\nChecking\nsignatures for\n%s...",
                    short_user);
            break;
        }

         case DISPLAY_STATE_SHOW_PRACTICES:
             if (practices_count == 1) {
                 strcpy(new_text, "\r\ndossier\nto sign!");
             } else {
                 strcpy(new_text, "\r\ndossiers\nto sign!");
             }
             break;
         case DISPLAY_STATE_NO_PRACTICES:
             strcpy(new_text, LV_SYMBOL_OK "\nNo dossiers\nto sign.\nRelax.");
             break;
         case DISPLAY_STATE_NO_WIFI_SLEEPING:
             strcpy(new_text, LV_SYMBOL_CLOSE "\nNo Wi-Fi.\nsleeping...");
             break;
         case DISPLAY_STATE_API_ERROR:
             strcpy(new_text, LV_SYMBOL_WARNING "\nAPI error!\nE-002");
             break;
         default:
             strcpy(new_text, LV_SYMBOL_BACKSPACE "\nUnknown state.\nE-003");
             break;
     }
 
     // If the main label does not exist, create it
     if (state_label == NULL) {
         state_label = lv_label_create(lv_scr_act());
         lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
         lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
         lv_obj_set_style_text_font(state_label, &lv_font_montserrat_28, 0);
         lv_obj_set_style_text_color(state_label, lv_color_white(), 0);
         lv_obj_set_style_text_align(state_label, LV_TEXT_ALIGN_CENTER, 0);
         lv_obj_align(state_label, LV_ALIGN_CENTER, 0, 20);

         // number_label in a larger font
         number_label = lv_label_create(lv_scr_act());
         lv_obj_set_style_text_font(number_label, &lv_font_montserrat_48, 0);
         lv_obj_set_style_text_color(number_label, lv_color_white(), 0);
         lv_obj_set_style_text_align(number_label, LV_TEXT_ALIGN_CENTER, 0);
         lv_obj_align(number_label, LV_ALIGN_CENTER, 0, -30);
         lv_obj_add_flag(number_label, LV_OBJ_FLAG_HIDDEN);
 
         lv_label_set_text(state_label, new_text);
         lv_obj_set_style_opa(state_label, 0, 0);
 
         // Fade-in at first startup
         fade_in_anim_start(state_label);
     }
    else {
        if (state == DISPLAY_STATE_CHECKING_API)
            pending_font = &lv_font_montserrat_18;
        else
            pending_font = &lv_font_montserrat_28;
    
        fade_out_anim_start(state_label);   // avvia fade-out, il callback cambierà il testo
    }

     // If we are in SHOW_PRACTICES, show number_label
     if (state == DISPLAY_STATE_SHOW_PRACTICES) {
         lv_label_set_text_fmt(number_label, "%d", practices_count);
         lv_obj_clear_flag(number_label, LV_OBJ_FLAG_HIDDEN);

     } else {
         // Hide number_label in other states
         lv_obj_add_flag(number_label, LV_OBJ_FLAG_HIDDEN);
     }

     // Animated arc management (states where needed)
     bool arc_needed = (state == DISPLAY_STATE_WARMING_UP ||
                        state == DISPLAY_STATE_BLE_ADVERTISING ||
                        state == DISPLAY_STATE_WIFI_CONNECTING ||
                        state == DISPLAY_STATE_CHECKING_API);
 
     if (arc_needed) {
         if (state_arc == NULL) {
             state_arc = lv_arc_create(lv_scr_act());
             lv_obj_remove_style(state_arc, NULL, LV_PART_KNOB);
             lv_arc_set_bg_angles(state_arc, 0, 360);
             lv_obj_set_style_arc_color(state_arc, lv_color_black(), LV_PART_MAIN);
             lv_obj_set_size(state_arc, LCD_H_RES, LCD_V_RES);
             lv_obj_align(state_arc, LV_ALIGN_CENTER, 0, 0);
             lv_obj_set_style_line_width(state_arc, ARC_LINE_WIDTH, 0);
             lv_obj_clear_flag(state_arc, LV_OBJ_FLAG_CLICKABLE);
             lv_arc_set_angles(state_arc, 0, ARC_SPAN);
 
             // Cancel any animations on arc
             lv_anim_del(state_arc, NULL);
 
             // Start animation
             lv_anim_t a_arc;
             lv_anim_init(&a_arc);
             lv_anim_set_var(&a_arc, state_arc);
             lv_anim_set_exec_cb(&a_arc, arc_anim_cb);
             lv_anim_set_values(&a_arc, 0, 360);
             lv_anim_set_time(&a_arc, 2000);
             lv_anim_set_repeat_count(&a_arc, LV_ANIM_REPEAT_INFINITE);
             lv_anim_start(&a_arc);
         }
         lv_color_t arc_color;
         if(state == DISPLAY_STATE_WARMING_UP) {
             arc_color = ARC_COLOR_WARMING;
         } else if(state == DISPLAY_STATE_BLE_ADVERTISING) {
             arc_color = ARC_COLOR_BLE;
         } else if(state == DISPLAY_STATE_WIFI_CONNECTING) {
             arc_color = ARC_COLOR_WIFI;
         } else {
             // state == DISPLAY_STATE_CHECKING_API
             arc_color = ARC_COLOR_API;
         }
         lv_obj_set_style_arc_color(state_arc, arc_color, LV_PART_INDICATOR);
     }
     else {
         // If not needed, remove the arc
         if (state_arc != NULL) {
             ESP_LOGI(TAG, "Removing arc...");
             lv_anim_del(state_arc, NULL);
             lv_obj_del(state_arc);
             state_arc = NULL;
         }
     }
 
     ESP_LOGI(TAG, "[DISPLAY] Stato aggiornato: %d, testo: \"%s\"",
              state, new_text);


     // Release the lock
     _lock_release(&lvgl_api_lock);
 }
 