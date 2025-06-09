#include "pulse.h"

static const char* TAG = "main:";

void adc_init() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11); 
}

void app_main() {
    adc_init(); 
    ESP_LOGW(TAG, "adc_init: done");
    initArduino();
    ESP_LOGW(TAG, "initArduino: done");
    lcd_setup();
    ESP_LOGW(TAG, "lcd_setup: done");
    init_buffers();
    ESP_LOGW(TAG, "init_buffers: done");
    init_timers();
    ESP_LOGW(TAG, "init_timers: done");
    init_tasks();
    ESP_LOGW(TAG, "init_tasks: done");
}