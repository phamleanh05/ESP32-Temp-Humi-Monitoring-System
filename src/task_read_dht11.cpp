#include "task_read_dht11.h"
#define DHTPIN 3
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void temp_humi_monitor(void *pvParameters){

    Serial.begin(115200);
    dht.begin();

    while (1){
        /* code */
        // Reading temperature in Celsius
        float temperature = dht.readTemperature();
        // Reading humidity
        float humidity = dht.readHumidity();

        

        // Check if any reads failed and exit early
        if (isnan(temperature) || isnan(humidity)) {
            Serial.println("Failed to read from DHT sensor!");
            temperature = humidity = -1;
        }

        //Update global variables for temperature and humidity
        glob_temperature = temperature;
        glob_humidity = humidity;

        // Print the results
        
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.print("%  Temperature: ");
        Serial.print(temperature);
        Serial.print("°C  Light: ");
        Serial.print(glob_light_level);
        Serial.print("  LED: ");
        Serial.println(glob_led_state ? "ON" : "OFF");
        vTaskDelay(5000);
    }
    
}