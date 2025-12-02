#ifndef __TASK_READ_DHT11__
#define __TASK_READ_DHT11__
#include <Arduino.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include "DHT.h"
#include "global.h"

void temp_humi_monitor(void *pvParameters);


#endif