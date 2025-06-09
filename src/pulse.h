#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/idf_additions.h>
#include <freertos/queue.h>

#include "driver/dac.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "soc/dac_channel.h"
#include "esp32/rom/ets_sys.h"
#include "freertos/queue.h"
#include "lcd.h"
#include <Arduino.h>

#define AVERAGE_WINDOW_SIZE 300
#define DEF_AVG 30

#define DECAY_WINDOW  0.00075f
#define DECAY_THRESHOLD  0.0005f
#define UPPER_FRAC   0.75f
#define LOWER_FRAC   0.40f

#define INPUT_LENGTH 7
#define OUTPUT_LENGTH 6

typedef struct {
    float a[2];
    float b[3];
    float x[3];
    float y[2];
} Biquad;

void init_timers();
void init_tasks();
void init_buffers();