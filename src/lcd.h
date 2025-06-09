#ifndef LCD_H
#define LCD_H

void lcd_setup();
void lcd_loop(float sensor_value, float sensor_min, float sensor_max, uint16_t current_bpm);

#endif // LCD_H