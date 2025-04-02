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

static const char *TAG = "display";

// Using SPI2 in the example
#define LCD_HOST  SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ     (15 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL  1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_SCLK           7
#define PIN_NUM_MOSI           9
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
#define LVGL_DRAW_BUF_LINES    40  // Puoi ridurlo se serve meno memoria
#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE   (8 * 1024)
#define LVGL_TASK_PRIORITY     2

extern const lv_font_t lv_font_montserrat_28;

// Mutex per proteggere le chiamate LVGL
static _lock_t lvgl_api_lock;

// Puntatore globale alla label LVGL per mostrare il testo degli stati
static lv_obj_t * state_label = NULL;
// Buffer globale per il nuovo testo da visualizzare
static char new_text[256] = {0};

// Callback per aggiornare l'opacità della label durante l'animazione fade out/in
static void anim_opacity_cb(void * var, int32_t v) {
    lv_obj_set_style_opa(state_label, v, 0);
}

// Callback per animazione di fade in
static void fade_in_anim_cb(void * var, int32_t v) {
    lv_obj_set_style_opa(state_label, v, 0);
}

// Avvia l'animazione di fade in (da 0 a 255 in 200ms)
static void fade_in_anim_start(void) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, state_label);
    lv_anim_set_exec_cb(&a, fade_in_anim_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 200);
    lv_anim_start(&a);
}

// Callback chiamata al termine dell'animazione di fade out.
// Qui viene aggiornato il testo e poi avviato il fade in.
static void fade_out_anim_ready_cb(lv_anim_t * a) {
    lv_label_set_text(state_label, new_text);
    lv_obj_align(state_label, LV_ALIGN_CENTER, 0, 0);
    fade_in_anim_start();
}

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                                      esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

/* Rotate display callback (già presente) */
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

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lvgl_port_update_callback(disp);
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
    
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel_gc9a01_draw_bitmap failed: %d", err);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t time_till_next_ms = 0;
    while (1) {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);
        if (time_till_next_ms < LVGL_TASK_MIN_DELAY_MS) {
            time_till_next_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
    }
}

void display_manager_init(void)
{
    ESP_LOGI(TAG, "Heap disponibile prima dell'inizializzazione display: %lu bytes", 
             (unsigned long) esp_get_free_heap_size());

    // Configura GPIO retroilluminazione come output e spegnila subito
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
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

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
    uint16_t *black_buffer = heap_caps_calloc(LCD_H_RES * lines_per_transfer, sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(black_buffer);

    for (int y = 0; y < LCD_V_RES; y += lines_per_transfer) {
        int lines = MIN(lines_per_transfer, LCD_V_RES - y);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + lines, black_buffer));
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

    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
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
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Heap disponibile dopo inizializzazione display: %lu bytes", 
             (unsigned long) esp_get_free_heap_size());
}

void display_manager_update(display_state_t state, int practices_count)
{
    switch (state) {
        case DISPLAY_STATE_WARMING_UP:
            ESP_LOGI(TAG, "[DISPLAY] Warming up... ⏰");
            strcpy(new_text, LV_SYMBOL_POWER "\nWarming up...");
            break;
        case DISPLAY_STATE_BLE_ADVERTISING:
            ESP_LOGI(TAG, "[DISPLAY] BLE Advertising - waiting for config...");
            strcpy(new_text, LV_SYMBOL_BLUETOOTH "\nBLE Active\nwaiting for\nconfig...");
            break;
        case DISPLAY_STATE_CONFIG_UPDATED:
            ESP_LOGI(TAG, "[DISPLAY] Configuration updated!");
            strcpy(new_text, LV_SYMBOL_OK "\nConfiguration\nupdated!");
            break;
        case DISPLAY_STATE_WIFI_CONNECTING:
            ESP_LOGI(TAG, "[DISPLAY] Connecting to Wi-Fi...");
            strcpy(new_text, LV_SYMBOL_WIFI "\nConnecting\nto Wi-Fi...");
            break;
        case DISPLAY_STATE_CHECKING_API:
            ESP_LOGI(TAG, "[DISPLAY] Checking API...");
            strcpy(new_text, LV_SYMBOL_REFRESH "\nChecking\nAPI...");
            break;
        case DISPLAY_STATE_SHOW_PRACTICES:
            if (practices_count == 1) {
                ESP_LOGI(TAG, "[DISPLAY] 1 dossier to sign!");
                snprintf(new_text, sizeof(new_text), "1\ndossier\nto sign!");
            } else {
                ESP_LOGI(TAG, "[DISPLAY] %d dossier to sign!", practices_count);
                snprintf(new_text, sizeof(new_text), "%d\ndossiers\nto sign!", practices_count);
            }
            break;
        case DISPLAY_STATE_NO_PRACTICES:
            ESP_LOGI(TAG, "[DISPLAY] No dossiers to sign.");
            strcpy(new_text, LV_SYMBOL_OK "\nNo dossier\nto sign.\nRelax.");
            break;
        case DISPLAY_STATE_NO_WIFI_SLEEPING:
            ESP_LOGI(TAG, "[DISPLAY] No Wi-Fi - sleeping...");
            strcpy(new_text, LV_SYMBOL_CLOSE "\nNo Wi-Fi\nsleeping...");
            break;
        case DISPLAY_STATE_API_ERROR:
            ESP_LOGI(TAG, "[DISPLAY] API error: unable to determine dossiers.");
            strcpy(new_text, LV_SYMBOL_WARNING "\nAPI error!");
            break;
        default:
            ESP_LOGW(TAG, "[DISPLAY] Unknown state.");
            strcpy(new_text, LV_SYMBOL_BACKSPACE "\nUnknown state.");
            break;
    }
    
    // Cancella eventuali animazioni pendenti per evitare accumuli
    if(state_label != NULL) {
        lv_anim_del(state_label, NULL);
    }
    
    if (state_label == NULL) {
        state_label = lv_label_create(lv_scr_act());
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
        lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
        lv_obj_set_style_text_font(state_label, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(state_label, lv_color_white(), 0);
        lv_obj_set_style_text_align(state_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(state_label, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(state_label, new_text);
        lv_obj_set_style_opa(state_label, 0, 0);
        // Avvia il fade in
        lv_anim_t a_in;
        lv_anim_init(&a_in);
        lv_anim_set_var(&a_in, state_label);
        lv_anim_set_exec_cb(&a_in, anim_opacity_cb);
        lv_anim_set_values(&a_in, 0, 255);
        lv_anim_set_time(&a_in, 200);
        lv_anim_start(&a_in);
    } else {
        // Fade out e poi aggiornamento
        lv_anim_t a_out;
        lv_anim_init(&a_out);
        lv_anim_set_var(&a_out, state_label);
        lv_anim_set_exec_cb(&a_out, anim_opacity_cb);
        lv_anim_set_values(&a_out, 255, 0);
        lv_anim_set_time(&a_out, 200);
        lv_anim_set_ready_cb(&a_out, fade_out_anim_ready_cb);
        lv_anim_start(&a_out);
    }
    
    ESP_LOGI(TAG, "[DISPLAY] Stato aggiornato: %d, testo: \"%s\"", state, new_text);
}

