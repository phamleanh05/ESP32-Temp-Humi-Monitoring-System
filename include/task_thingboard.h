#ifndef __TASK_THINGBOARD_H__
#define __TASK_THINGBOARD_H__

#include <Arduino.h>
#include <WiFi.h>
#include <ThingsBoard.h>
#include <Arduino_MQTT_Client.h>
#include <Adafruit_NeoPixel.h>
#include "global.h"

// LED control pins - use existing pin definitions
#define TB_BOARD_LED_PIN 2       // Built-in LED
#define TB_EXTERNAL_LED_PIN 25   // External LED 
#define TB_NEOPIXEL_PIN 45       // Use existing NEO_PIN value

// Main task function
void task_thingboard(void *pvParameters);

// Initialization functions
void initThingBoardTask();
void initThingBoardLEDs();

// LED Control functions
void controlThingBoardLED(const String& ledName, bool state);
void sendTelemetryData();

// RPC callback functions
RPC_Response processLEDControl(const RPC_Data &data);
RPC_Response processGetStates(const RPC_Data &data);

// ThingBoard specific LED states
extern bool tb_board_led_state;
extern bool tb_external_led_state; 
extern bool tb_neopixel_state;

#endif