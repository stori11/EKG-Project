#include "pulse.h"

static TaskHandle_t sampling_task;

portMUX_TYPE ecg_mux = portMUX_INITIALIZER_UNLOCKED;
static uint16_t beat_detected = 0;
static uint16_t beat_count = 0;
static uint16_t bpm = 0;

static float window_min = 0;
static float window_max = 0;
static float threshold_base = 0;
static float threshold_base_upper = 0;
static float upper_threshold = 0.0f;
static float lower_threshold = 0.0f;

static int buffer_average_i = 0;
static float buffer_average[AVERAGE_WINDOW_SIZE] = {DEF_AVG}; 
static float sum = 0.0f;

static float sensor_raw_received = 0.0f; 
static float sensor_offset = 0.0f;
static float sensor_filtered = 0.0f;

Biquad filter[3];

static float coefficients[3][5] = {{2.23775228e-06f, 4.47550457e-06f, 2.23775228e-06f, -1.97693169f, 0.977817352f}, 
{1.0f, 0.0f, -1.0f, -1.97379693f, 0.973952795f}, 
{1.0f, -2.0f, 1.0f, -1.99602456f,  0.996052349f}};

float calculate_average(float sample) {
    // Subtract the oldest sample from the sum
    sum -= buffer_average[buffer_average_i];
    // Replace it with the new sample
    buffer_average[buffer_average_i] = sample;
    // Add the new sample to the sum
    sum += sample;
    // Update the circular index
    buffer_average_i = (buffer_average_i + 1) % AVERAGE_WINDOW_SIZE;
    // Substract the offset
    float average = sum / AVERAGE_WINDOW_SIZE;
    return average;
}

void calculate_thresholds(float sample)
{
    //Check if the current sample is higher than previous max value for both window and threshold
    if (sample > window_max) {
        //If higher update both to the current sample
        window_max = sample;
        threshold_base_upper = sample;
    } else {            
        //If not the current max window and threshold is decremented by a percentage of the difference between the current sample and the previous max              
        window_max += DECAY_WINDOW * (sample - window_max);
        threshold_base_upper += DECAY_THRESHOLD * (sample - threshold_base_upper);
    }
    //Check if the current sample is lower than previous min value for both window and threshold
    if (sample < window_min) {
        //If lower update both to the current sample
        window_min = sample;
        threshold_base = sample;
    } else {                      
        //If not the current min window and threshold is decremented by a percentage of the difference between the current sample and the previous min    
        window_min += DECAY_WINDOW * (sample - window_min);
        threshold_base += DECAY_THRESHOLD * (sample - threshold_base);
    }
    //Calculate the width of the band between the upper and lower threshold base
    const float signal_width   =  threshold_base_upper - threshold_base;
    //Calculate new thresholds as the base + fraction of the band between the bases
    upper_threshold = threshold_base + UPPER_FRAC * signal_width;
    lower_threshold = threshold_base + LOWER_FRAC * signal_width; 
}

static inline float biquad_filter(Biquad *filter, float new_input) {
    //Output for the filter which will be returned later
    float new_output = 0.0f;
    //Stored inputs are moved to the right in the array discarding the oldest input
    for (int i = 2; i > 0; i--) {
        filter->x[i] = filter->x[i-1];
    }
    //New sample is stored into the array on the first index
    filter->x[0] = new_input;
    //Current and past inputs are multiplied with the corresponding coefficients and added to the current output
    for (int i = 0; i < 3; i++) {
        new_output += (filter->b[i]*filter->x[i]); 
    }
    //Past outputs are multiplied with the corresponding coefficients and substracted from the current output
    for (int i = 0; i < 2; i++) {
        new_output -= (filter->a[i]*filter->y[i]);
    }
    //Stored outputs are moved to the right in the array discarding the oldest output
    for (int i = 1; i > 0; i--) {
        filter->y[i] = filter->y[i-1];
    }
    //Current output is stored in the array on the first index
    filter->y[0] = new_output;
    return new_output;
}

void process_sensor_values() {
    //Current reading is normalized by substracting the average of the last 300 samples
    sensor_offset = (sensor_raw_received - calculate_average(sensor_raw_received));
    //Normalized reading is passed through the buttersworth filter implemented as 3 biquads
    float sensor_filtered_1 = biquad_filter(&filter[0], sensor_offset);
    float sensor_filtered_2 = biquad_filter(&filter[1], sensor_filtered_1);
    sensor_filtered = -biquad_filter(&filter[2], sensor_filtered_2);
    //Filtered reading is used to establish new thresholds for beat detection and the signal window for the display
    calculate_thresholds(sensor_filtered);
    //Check if signal is over the threshold and if the FSM is currently not inside a beat
    if (sensor_filtered > upper_threshold && beat_detected == 0) {
        //Spinlock critical section for the beat_count variable as its written into by this task and a timer
        portENTER_CRITICAL(&ecg_mux);
        //Beat count is incremented
        beat_count++;
        //Exit critical section
        portEXIT_CRITICAL(&ecg_mux);
        //Set state to inside a beat
        beat_detected = 1;
    }
    //Check if signal is under the lower threshold to detect "exiting" a beat and if the FSM is currently inside a beat
    if (sensor_filtered < lower_threshold && beat_detected == 1) {
        //Set state to outside a beat
        beat_detected = 0;
    }
}


void process_sensor_task(void *pvParameters) {
    while(1) {
        //Task blocks until a new sample arrives notified by the timer
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        //Read a new sample from the ADC
        sensor_raw_received = adc1_get_raw(ADC_CHANNEL_4);
        //Process the sample through normalization, filtering, threshold updates and beat detection
        process_sensor_values();        
    }
}


void IRAM_ATTR input_timer_callback(void *arg) {
    //Variable used by the notification call to unblock the sample processing task
    BaseType_t task_woken = pdFALSE;
    //Notify the task to read a new sample
    vTaskNotifyGiveFromISR(sampling_task, &task_woken);
    //Yield to task
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR timer_bpm(void *arg) {
    //Spinlock critical section for the beat_count variable as its written into by this task and a timer
    portENTER_CRITICAL(&ecg_mux);
    //Copy the beat count for BPM calculation
    uint16_t count = beat_count;
    //Reset the count
    beat_count = 0;
    //Exit the critical section
    portEXIT_CRITICAL(&ecg_mux);
    //Calculate the BPM by multiplying by 60 (corresponding to 1 minute period) and divide by the calculation frequency (10s)
    bpm = (count*60)/10;
}

void init_timers() {
    const esp_timer_create_args_t input_timer_args = {
        .callback = &input_timer_callback,
        .name = "periodic"};

    esp_timer_handle_t input_timer;
    ESP_ERROR_CHECK(esp_timer_create(&input_timer_args, &input_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(input_timer, 1000));

    const esp_timer_create_args_t input_timer_args2 = {
        .callback = &timer_bpm,
        .name = "2"};

    esp_timer_handle_t input_timer2;
    ESP_ERROR_CHECK(esp_timer_create(&input_timer_args2, &input_timer2));
    ESP_ERROR_CHECK(esp_timer_start_periodic(input_timer2, 10000000));
}

void lcdTask(void *pvParameters) {
    while(1) {
        //Pass filtered sensor value, signal window and bpm to display
        lcd_loop(sensor_filtered, window_min, window_max, bpm);
    }
}


void init_tasks() {
    //Initialize the task passing values to the display
    xTaskCreate(lcdTask, "LCD Task", 2048, NULL, 3, NULL);
    //Initialize the task reading and processing samples, with a handle used by the timer notifications
    xTaskCreate(process_sensor_task, "BPM Task", 2048, NULL, 5, &sampling_task);
}

void init_buffers() {
    //Fill the buffer with a default value to avoid uninitialized garbage
    for (int i = 0; i < AVERAGE_WINDOW_SIZE; i++) {
        buffer_average[i] = DEF_AVG;
    }
    //Fill the 3 biquad structs with corresponding coefficients, initialize input and output arrays with 0s
    for (int i = 0; i < 3; i++) {
        filter[i].b[0] = coefficients[i][0];
        filter[i].b[1] = coefficients[i][1];
        filter[i].b[2] = coefficients[i][2];

        filter[i].a[0] = coefficients[i][3];
        filter[i].a[1] = coefficients[i][4];
        
        filter[i].x[0] = 0.0f;
        filter[i].x[1] = 0.0f;
        filter[i].x[2] = 0.0f;

        filter[i].y[0] = 0.0f;
        filter[i].y[1] = 0.0f;
    }
}