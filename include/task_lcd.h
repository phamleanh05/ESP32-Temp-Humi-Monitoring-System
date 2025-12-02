#ifndef __TASK_LCD_H__
#define __TASK_LCD_H__

#include <Arduino.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include "global.h"

#define LCD_ADDR 33
#define LCD_COLS 16
#define LCD_ROWS 2
#define SDA_PIN 11
#define SCL_PIN 12

void task_lcd(void *pvParameters);
void initLCD();
void displaySensorData();

#endif