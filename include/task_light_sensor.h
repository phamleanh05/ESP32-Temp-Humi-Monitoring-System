#ifndef __TASK_LIGHT_SENSOR_H__
#define __TASK_LIGHT_SENSOR_H__

#include <Arduino.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include "global.h"

#define LIGHT_SENSOR_PIN 1    // GPIO36 (ADC1_CH0) for light sensor
#define LED_PIN 2              // GPIO pin for LED control
#define LIGHT_THRESHOLD 500    // Threshold value for darkness detection

void task_light_sensor(void *pvParameters);
void initLightSensor();
int readLightLevel();
void controlLED(bool state);

extern int glob_light_level;
extern bool glob_led_state;

#endif