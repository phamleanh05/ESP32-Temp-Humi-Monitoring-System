#include "task_thingboard.h"
#include "task_light_sensor.h"

// External ThingBoard objects from task_core_iot.cpp
extern WiFiClient wifiClient;
extern Arduino_MQTT_Client mqttClient;
extern ThingsBoard tb;

// ThingBoard LED states
bool tb_board_led_state = false;
bool tb_external_led_state = false; 
bool tb_neopixel_state = false;

// NeoPixel for ThingBoard control
Adafruit_NeoPixel tb_neopixel(1, TB_NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// RPC callbacks for LED control
const std::array<RPC_Callback, 2U> thingboard_callbacks = {{
  { "controlLED", processLEDControl },
  { "getStates", processGetStates }
}};

// Timing variables
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 10000; // 10 seconds

void task_thingboard(void *pvParameters) {
  Serial.println("ThingBoard task starting...");
  
  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("ThingBoard: Waiting for WiFi...");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  
  initThingBoardTask();
  
  while (true) {
    // Check WiFi connection
    if (WiFi.status() == WL_CONNECTED) {
      
      // Connect to ThingBoard if not connected
      if (!tb.connected()) {
        Serial.println("Connecting to ThingBoard...");
        if (tb.connect(CORE_IOT_SERVER.c_str(), CORE_IOT_TOKEN.c_str(), CORE_IOT_PORT.toInt())) {
          Serial.println("ThingBoard connected!");
          
          // Send device attributes
          tb.sendAttributeData("deviceName", "ESP32 Sensor Hub");
          tb.sendAttributeData("firmwareVersion", "1.0.0");
          tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
          
          // Subscribe to RPC commands
          if (!tb.RPC_Subscribe(thingboard_callbacks.cbegin(), thingboard_callbacks.cend())) {
            Serial.println("Failed to subscribe to ThingBoard RPC");
          } else {
            Serial.println("ThingBoard RPC subscribed successfully");
          }
        } else {
          Serial.println("ThingBoard connection failed");
        }
      }
      
      // Send telemetry data periodically
      if (tb.connected()) {
        unsigned long currentTime = millis();
        
        if (currentTime - lastTelemetryTime >= TELEMETRY_INTERVAL) {
          sendTelemetryData();
          lastTelemetryTime = currentTime;
        }
        
        // Process ThingBoard messages
        tb.loop();
      }
    }
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void initThingBoardTask() {
  Serial.println("Initializing ThingBoard task...");
  initThingBoardLEDs();
}

void initThingBoardLEDs() {
  // Initialize LED pins
  pinMode(TB_BOARD_LED_PIN, OUTPUT);
  pinMode(TB_EXTERNAL_LED_PIN, OUTPUT);
  digitalWrite(TB_BOARD_LED_PIN, LOW);
  digitalWrite(TB_EXTERNAL_LED_PIN, LOW);
  
  // Initialize NeoPixel
  tb_neopixel.begin();
  tb_neopixel.clear();
  tb_neopixel.show();
  
  Serial.println("ThingBoard LEDs initialized");
}

void sendTelemetryData() {
  // DHT11 sensor data
  tb.sendTelemetryData("temperature", glob_temperature);
  tb.sendTelemetryData("humidity", glob_humidity);
  tb.sendTelemetryData("temp_valid", !isnan(glob_temperature));
  tb.sendTelemetryData("humi_valid", !isnan(glob_humidity));
  
  // Light sensor data
  tb.sendTelemetryData("light_level", glob_light_level);
  tb.sendTelemetryData("light_led_auto", glob_led_state);
  
  // ThingBoard controlled LED states
  tb.sendTelemetryData("board_led", tb_board_led_state);
  tb.sendTelemetryData("external_led", tb_external_led_state);
  tb.sendTelemetryData("neopixel", tb_neopixel_state);
  
  // System information
  tb.sendTelemetryData("uptime", millis() / 1000);
  tb.sendTelemetryData("free_heap", ESP.getFreeHeap());
  tb.sendTelemetryData("wifi_rssi", WiFi.RSSI());
  
  Serial.println("Telemetry sent to ThingBoard");
}

void controlThingBoardLED(const String& ledName, bool state) {
  if (ledName == "board") {
    tb_board_led_state = state;
    digitalWrite(TB_BOARD_LED_PIN, state ? HIGH : LOW);
    Serial.printf("ThingBoard Board LED: %s\n", state ? "ON" : "OFF");
    
  } else if (ledName == "external") {
    tb_external_led_state = state;
    digitalWrite(TB_EXTERNAL_LED_PIN, state ? HIGH : LOW);
    Serial.printf("ThingBoard External LED: %s\n", state ? "ON" : "OFF");
    
  } else if (ledName == "neopixel") {
    tb_neopixel_state = state;
    if (state) {
      tb_neopixel.setPixelColor(0, tb_neopixel.Color(0, 255, 0)); // Green
    } else {
      tb_neopixel.setPixelColor(0, tb_neopixel.Color(0, 0, 0)); // Off
    }
    tb_neopixel.show();
    Serial.printf("ThingBoard NeoPixel: %s\n", state ? "ON" : "OFF");
  }
}

// RPC Callback: Control LEDs
RPC_Response processLEDControl(const RPC_Data &data) {
  Serial.println("ThingBoard LED control RPC received");
  
  String ledName = data["led"];
  bool state = data["state"];
  
  controlThingBoardLED(ledName, state);
  
  StaticJsonDocument<128> response;
  response["success"] = true;
  response["led"] = ledName;
  response["state"] = state;
  response["message"] = "LED controlled successfully";
  
  return RPC_Response(response);
}

// RPC Callback: Get all states
RPC_Response processGetStates(const RPC_Data &data) {
  Serial.println("ThingBoard get states RPC received");
  
  StaticJsonDocument<256> response;
  response["success"] = true;
  
  // LED states
  response["board_led"] = tb_board_led_state;
  response["external_led"] = tb_external_led_state;
  response["neopixel"] = tb_neopixel_state;
  response["light_auto_led"] = glob_led_state;
  
  // Sensor data
  response["temperature"] = glob_temperature;
  response["humidity"] = glob_humidity;
  response["light_level"] = glob_light_level;
  
  // System info
  response["uptime"] = millis() / 1000;
  response["free_heap"] = ESP.getFreeHeap();
  response["wifi_rssi"] = WiFi.RSSI();
  
  return RPC_Response(response);
}