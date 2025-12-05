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
    } else if (displayMode == 1) {
        // Display Light level and LED status
        lcd.setCursor(0, 0);
        lcd.print("Light: ");
        lcd.print(glob_light_level);
        lcd.setCursor(0, 1);
        lcd.print("LED: ");
        lcd.print(glob_led_state ? "ON " : "OFF");
    } else {
        // Display ESP32 IP Address
        lcd.setCursor(0, 0);
        if (WiFi.status() == WL_CONNECTED) {
            lcd.print("WiFi IP:");
            lcd.setCursor(0, 1);
            String ip = WiFi.localIP().toString();
            if (ip.length() > 16) {
                // If IP is too long, scroll it or show shortened version
                lcd.print(ip.substring(0, 16));
            } else {
                lcd.print(ip);
            }
        } else {
            lcd.print("No WiFi");
            lcd.setCursor(0, 1);
            lcd.print("192.168.4.1"); // Access Point IP
        }
    }
    
    // Switch display mode every cycle (3 modes now)
    displayMode = (displayMode + 1) % 3;
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