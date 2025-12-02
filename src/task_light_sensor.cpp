#include "task_light_sensor.h"

int glob_light_level = 0;
bool glob_led_state = false;

void initLightSensor() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.println("Light sensor and LED initialized");
}

int readLightLevel() {
    return analogRead(LIGHT_SENSOR_PIN);
}

void controlLED(bool state) {
    digitalWrite(LED_PIN, state ? HIGH : LOW);
    glob_led_state = state;
    Serial.printf("LED %s\n", state ? "ON" : "OFF");
}

void task_light_sensor(void *pvParameters) {
    initLightSensor();
    
    Serial.println("Light sensor task started");
    Serial.printf("Light threshold: %d\n", LIGHT_THRESHOLD);
    Serial.printf("LED GPIO: %d\n", LED_PIN);
    Serial.printf("Light sensor GPIO: %d\n", LIGHT_SENSOR_PIN);
    
    while (1) {
        // Read light level from sensor
        int lightLevel = readLightLevel();
        glob_light_level = lightLevel;
        
        // Check if it's dark (light level below threshold)
        if (lightLevel < LIGHT_THRESHOLD) {
            // Turn on LED when it's dark
            if (!glob_led_state) {
                controlLED(true);
                Serial.printf("Dark detected (light: %d) - LED turned ON\n", lightLevel);
            }
        } else {
            // Turn off LED when there's enough light
            if (glob_led_state) {
                controlLED(false);
                Serial.printf("Light detected (light: %d) - LED turned OFF\n", lightLevel);
            }
        }
        
        // Print current status periodically
        Serial.printf("Light level: %d, LED: %s\n", lightLevel, glob_led_state ? "ON" : "OFF");
        
        // Wait for 2 seconds before next reading
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}