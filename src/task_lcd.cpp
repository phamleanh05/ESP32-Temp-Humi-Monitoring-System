#include "task_lcd.h"

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

void initLCD() {
    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.begin();
    lcd.backlight();
    Serial.println("LCD initialized");
}

void displaySensorData() {
    static int displayMode = 0;
    
    lcd.clear();
    
    if (displayMode == 0) {
        // Display Temperature and Humidity
        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(glob_temperature, 1);
        lcd.print("C");
        lcd.setCursor(0, 1);
        lcd.print("Humi: ");
        lcd.print(glob_humidity, 1);
        lcd.print("%");
    } else {
        // Display Light level and LED status
        lcd.setCursor(0, 0);
        lcd.print("Light: ");
        lcd.print(glob_light_level);
        lcd.setCursor(0, 1);
        lcd.print("LED: ");
        lcd.print(glob_led_state ? "ON " : "OFF");
    }
    
    // Switch display mode every cycle
    displayMode = (displayMode + 1) % 2;
}

void task_lcd(void *pvParameters) {
    initLCD();
    
    Serial.println("LCD display task started");
    Serial.printf("LCD Address: 0x%02X, Size: %dx%d\n", LCD_ADDR, LCD_COLS, LCD_ROWS);
    Serial.printf("I2C Pins - SDA: %d, SCL: %d\n", SDA_PIN, SCL_PIN);
    
    while (1) {
        // Update LCD display with sensor data
        displaySensorData();
        
        // Wait for 3 seconds before next update
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}