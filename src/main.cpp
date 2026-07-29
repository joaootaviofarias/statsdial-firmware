#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "esp_task_wdt.h"

#include "shared_types.h"
#include "wait_ui.h"
#include "clock_ui.h"
#include "monitor_ui.h"

#define DISP_W     240
#define DISP_H     240
#define BUF_LINES   20

static TFT_eSPI         tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t         lvgl_buf[DISP_W * BUF_LINES];

typedef enum {
    STATE_WAITING,
    STATE_MONITOR,
    STATE_CLOCK
} system_state_t;

static system_state_t current_state = STATE_WAITING;
static uint32_t last_packet_time = 0;

static uint8_t  clk_hour = 0;
static uint8_t  clk_minute = 0;
static uint8_t  clk_second = 0;
static uint32_t last_clock_tick_ms = 0;
static bool     has_time_sync = false;

static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(reinterpret_cast<uint16_t *>(color_p), w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(drv);
}

static void lvgl_tick_task(void * /*arg*/) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5));
        lv_tick_inc(5);
    }
}

static bool process_serial_input(monitor_data_t *out_data) {
    static char rxbuf[48];
    static int  rxpos = 0;
    bool new_packet_parsed = false;

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            rxbuf[rxpos] = '\0'; 
            rxpos = 0;
            
            int cpu, gpu, ram, tmp, hr, mn, sc;
            if (sscanf(rxbuf, "%d,%d,%d,%d,%d,%d,%d", &cpu, &gpu, &ram, &tmp, &hr, &mn, &sc) == 7) {
                out_data->cpu_pct    = (uint8_t)cpu;
                out_data->gpu_pct    = (uint8_t)gpu;
                out_data->ram_pct    = (uint8_t)ram;
                out_data->cpu_temp_c = (uint8_t)tmp;
                out_data->hour       = (uint8_t)hr;
                out_data->minute     = (uint8_t)mn;
                out_data->second     = (uint8_t)sc;
                new_packet_parsed = true;
            }
        } else if (rxpos < (int)sizeof(rxbuf) - 1) {
            rxbuf[rxpos++] = c;
        }
    }
    return new_packet_parsed;
}

void setup(void) {
    Serial.begin(115200);
    Serial.println("[monitor] booting");

    esp_task_wdt_init(5, true);
    esp_task_wdt_add(NULL); 

    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, lvgl_buf, NULL, DISP_W * BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = DISP_W;
    disp_drv.ver_res  = DISP_H;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    xTaskCreate(
        lvgl_tick_task, "lv_tick",
        2048, NULL,
        configMAX_PRIORITIES - 1,
        NULL
    );

    // Initialize all UIs
    wait_ui_init();
    clock_ui_init();
    monitor_ui_init();
    
    // Start on the Wait Screen
    wait_ui_show();
    
    Serial.println("[monitor] UI Multi-Screen Environment Ready");
}

void loop(void) {
    monitor_data_t stats;
    uint32_t now = millis();

    esp_task_wdt_reset();

    if (process_serial_input(&stats)) {
        last_packet_time = now;
        
        clk_hour   = stats.hour;
        clk_minute = stats.minute;
        clk_second = stats.second;
        last_clock_tick_ms = now;
        has_time_sync = true;

        if (current_state != STATE_MONITOR) {
            monitor_ui_show();
            current_state = STATE_MONITOR;
        }
        monitor_ui_update(&stats);
    }

    if (current_state == STATE_MONITOR && (now - last_packet_time > 3000)) {
        if (has_time_sync) {
            clock_ui_update(clk_hour, clk_minute, clk_second);
            clock_ui_show();
            current_state = STATE_CLOCK;
        } else {
            wait_ui_show();
            current_state = STATE_WAITING;
        }
    }

    if (current_state == STATE_CLOCK) {
        if (now - last_clock_tick_ms >= 1000) {
            uint32_t seconds_passed = (now - last_clock_tick_ms) / 1000;
            last_clock_tick_ms += seconds_passed * 1000;

            clk_second += seconds_passed;
            if (clk_second >= 60) {
                clk_minute += clk_second / 60;
                clk_second %= 60;
                if (clk_minute >= 60) {
                    clk_hour += clk_minute / 60;
                    clk_minute %= 60;
                    if (clk_hour >= 24) clk_hour %= 24;
                }
            }

            clock_ui_update(clk_hour, clk_minute, clk_second);
        }
    }

    lv_timer_handler();
    delay(5);
}