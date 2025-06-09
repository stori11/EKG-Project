/*
You need to install Adafruit SSD1306 library to work with OLED
See a guide here: https://arduinogetstarted.com/tutorials/arduino-oled
*/

/*
You may alsowant to look at the example code on how to work with pulse sensor:
https://pulsesensor.com/pages/installing-our-playground-for-pulsesensor-arduino
*/
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width,  in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// declare an SSD1306 display object connected to I2C
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

int x=0;                   // current position of the cursor
int y=0;
int lastx=0;                // last position of the cursor
int lasty=0;

extern "C" void lcd_setup() {
    Wire.begin(21, 22);

  // initialize OLED display with address 0x3C for 128x64
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }

  delay(2000);         // wait for initializing
  oled.clearDisplay(); // clear display

  oled.setTextSize(1);          // text size
  oled.setTextColor(WHITE);     // text color
  oled.setCursor(0, 0);        // position to display
  oled.println("Starting..."); // text to display
  oled.display();               // show on OLED
  delay(2000);
  oled.clearDisplay();        // clear display
  oled.setCursor(0, 0);
  oled.print("BPM: ");
}


extern "C" void lcd_loop(float sensor_value, float sensor_min, float sensor_max, uint16_t current_bpm) {

  if(x>SCREEN_WIDTH - 1){                    // reset the screen when cursor reaches the border of the LED screen
      oled.clearDisplay();
      x=0;
      lastx=x;
  }
  
  //Black rectangle to cover a small area for the BPM display, cutting of a small part of the signal display in the left corner of the display
  oled.fillRect(0, 0, 42, 8, BLACK);
  //Cursor set to top left corder
  oled.setCursor(0, 0);
  //Label for the BPM display
  oled.print("BPM:");
  //Cursor set to right past the BPM label
  oled.setCursor(23, 0);
  //Print out current BPM value passed by the display task
  oled.print(current_bpm);
  //Signal window continuously calculated based on the signal
  float lower_bound = sensor_min;
  float upper_bound = sensor_max;
  float conversion = (upper_bound - lower_bound) / float(SCREEN_HEIGHT);

  
  y = (SCREEN_HEIGHT)-((sensor_value-lower_bound)/conversion)-1;              // strength of the signal in the screen coordinates
  oled.writeLine(lastx,lasty,x,y,WHITE);                              // write a line between previous and the current cursor positions
  
  lasty=y;
  lastx=x;
  x++;
  oled.display();
  delay(10);
}
