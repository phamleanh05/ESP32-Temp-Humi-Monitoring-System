#include "task_read_dht11.h"
#define DHTPIN 1
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(33,16,2);

void temp_humi_monitor(void *pvParameters){

    Serial.begin(115200);
    dht.begin();
    Wire.begin(11,12);
    lcd.begin();
    lcd.backlight();

    while (1){
        /* code */
        // Reading temperature in Celsius
        float temperature = dht.readTemperature();
        // Reading humidity
        float humidity = dht.readHumidity();

        

        // Check if any reads failed and exit early
        if (isnan(temperature) || isnan(humidity)) {
            Serial.println("Failed to read from DHT sensor!");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("DHT Sensor Error");
            temperature = humidity =  -1;
            //return;
        } else {
            // Display on LCD
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Temp: ");
            lcd.print(temperature, 1);
            lcd.print("C");
            lcd.setCursor(0, 1);
            lcd.print("Humi: ");
            lcd.print(humidity, 1);
            lcd.print("%");
        }

        //Update global variables for temperature and humidity
        glob_temperature = temperature;
        glob_humidity = humidity;

        // Print the results
        
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.print("%  Temperature: ");
        Serial.print(temperature);
        Serial.println("°C");
        vTaskDelay(5000);
    }
    
}