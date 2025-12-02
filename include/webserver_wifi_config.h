#ifndef WEBSERVER_WIFI_CONFIG_H
#define WEBSERVER_WIFI_CONFIG_H

#include <WiFi.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include "global.h"
#include "task_check_info.h"

#define LED_GPIO 48
#define NEO_PIN 45
#define LED_COUNT 1

struct WiFiCredentials {
    String ssid;
    String password;
};

struct WiFiNetwork {
    String ssid;
    int rssi;
    bool secured;
};

class WiFiConfigServer {
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    bool isConfigMode;
    String configSSID;
    String configPassword;
    
    // LED control
    bool ledState;
    bool neoState;
    Adafruit_NeoPixel* neoPixel;
    
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                   AwsEventType type, void *arg, uint8_t *data, size_t len);
    void setupConfigRoutes();
    String getWiFiScanJSON();
    String getWiFiStatusJSON();
    String getConfigPageHTML();
    
public:
    WiFiConfigServer(AsyncWebServer* webServer, AsyncWebSocket* webSocket);
    
    void begin();
    void loop();
    void startConfigMode(const char* apSSID = "ESP32-Config", const char* apPassword = "12345678");
    void stopConfigMode();
    
    bool saveWiFiCredentials(const String& ssid, const String& password);
    WiFiCredentials loadWiFiCredentials();
    void clearWiFiCredentials();
    
    std::vector<WiFiNetwork> scanWiFiNetworks();
    bool connectToWiFi(const String& ssid, const String& password, unsigned long timeout = 10000);
    void disconnectWiFi();
    
    bool isConnected();
    String getConnectedSSID();
    String getLocalIP();
    int getRSSI();
    
    void sendWiFiStatus();
    void sendWiFiList();
    void sendSensorData();
    void sendLEDStatus();
    void broadcastMessage(const String& message);
    String getSensorDataJSON();
    String getLEDStatusJSON();
    
    // LED control methods
    void setLEDState(bool state);
    void setNeoState(bool state);
    bool getLEDState();
    bool getNeoState();
};

extern WiFiConfigServer* wifiConfig;

// WiFi configuration task function
void webserver_wifi_config_task(void *parameter);

#endif