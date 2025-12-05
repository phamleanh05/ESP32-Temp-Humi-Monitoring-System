#include "webserver_wifi_config.h"

WiFiConfigServer* wifiConfig = nullptr;

// WiFi config server instances
AsyncWebServer wifiConfigServer(8080);
AsyncWebSocket wifiConfigWS("/ws");

// WiFi configuration task
void webserver_wifi_config_task(void *parameter)
{
  // Initialize WiFi config
  wifiConfig = new WiFiConfigServer(&wifiConfigServer, &wifiConfigWS);
  wifiConfig->begin();
  
  Serial.println("WiFi Configuration Server started on port 8080");
  Serial.println("Access: http://192.168.4.1:8080 (in AP mode)");
  
  while (true)
  {
    wifiConfig->loop();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

WiFiConfigServer::WiFiConfigServer(AsyncWebServer* webServer, AsyncWebSocket* webSocket) 
    : server(webServer), ws(webSocket), isConfigMode(false), ledState(false), neoState(true),
      savedNeoR(0), savedNeoG(255), savedNeoB(0), savedNeoHex("#00ff00"),
      alertNeoR(255), alertNeoG(0), alertNeoB(0), alertNeoHex("#ff0000"), tempThreshold(30.0),
      isBlinking(false), lastBlinkTime(0), blinkState(false) {
    // Initialize LED pins
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LOW);
    
    // Initialize NeoPixel - start with normal color (green)
    neoPixel = new Adafruit_NeoPixel(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    neoPixel->begin();
    neoPixel->setPixelColor(0, neoPixel->Color(savedNeoR, savedNeoG, savedNeoB)); // Start with saved color
    neoPixel->show();
}

void WiFiConfigServer::begin() {
    preferences.begin("wifi-config", false);
    
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    
    // Load saved NeoPixel color and alert settings
    loadSavedNeoColor();
    loadAlertSettings();
    
    ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWsEvent(server, client, type, arg, data, len);
    });
    
    server->addHandler(ws);
    setupConfigRoutes();
    
    // Start the web server
    server->begin();
    Serial.println("WiFi Config Web Server started");
    
    WiFiCredentials creds = loadWiFiCredentials();
    if (creds.ssid.length() > 0) {
        if (!connectToWiFi(creds.ssid, creds.password)) {
            Serial.println("Failed to connect with saved credentials, starting config mode");
            startConfigMode();
        }
    } else {
        startConfigMode();
    }
}

void WiFiConfigServer::loop() {
    if (isConfigMode) {
        if (WiFi.softAPgetStationNum() == 0) {
            static unsigned long lastCheck = 0;
            if (millis() - lastCheck > 30000) {
                Serial.println("No clients connected to config AP");
                lastCheck = millis();
            }
        }
    }
    
    static unsigned long lastStatusUpdate = 0;
    static unsigned long lastSensorUpdate = 0;
    
    if (millis() - lastStatusUpdate > 5000) {
        sendWiFiStatus();
        lastStatusUpdate = millis();
    }
    
    // Send sensor data including light sensor every 3 seconds
    if (millis() - lastSensorUpdate > 3000) {
        sendSensorData();
        
        // Update NeoPixel based on temperature
        if (!isnan(glob_temperature)) {
            setNeoColorForTemperature(glob_temperature);
        }
        
        lastSensorUpdate = millis();
    }
    
    // Handle NeoPixel blinking for alerts
    handleNeoBlinking();
}

void WiFiConfigServer::startConfigMode(const char* apSSID, const char* apPassword) {
    Serial.println("Starting WiFi Config Mode");
    
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID, apPassword);
    
    configSSID = apSSID;
    configPassword = apPassword;
    isConfigMode = true;
    
    Serial.print("Config AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.printf("Connect to '%s' with password '%s'\n", apSSID, apPassword);
    Serial.println("Then navigate to http://192.168.4.1:8080");
}

void WiFiConfigServer::stopConfigMode() {
    if (isConfigMode) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        isConfigMode = false;
        Serial.println("WiFi Config Mode stopped");
    }
}

bool WiFiConfigServer::saveWiFiCredentials(const String& ssid, const String& password) {
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    // Save_info_File(ssid, password, CORE_IOT_TOKEN.isEmpty() ? "" : CORE_IOT_TOKEN, 
    //                CORE_IOT_SERVER.isEmpty() ? "" : CORE_IOT_SERVER, 
    //                CORE_IOT_PORT.isEmpty() ? "" : CORE_IOT_PORT);

    Serial.printf("WiFi credentials saved: %s\n", ssid.c_str());
    return true;
}

WiFiCredentials WiFiConfigServer::loadWiFiCredentials() {
    WiFiCredentials creds;
    creds.ssid = preferences.getString("ssid", "");
    creds.password = preferences.getString("password", "");
    // creds.ssid = WIFI_SSID;
    // creds.password = WIFI_PASS;
    return creds;
}


std::vector<WiFiNetwork> WiFiConfigServer::scanWiFiNetworks() {
    std::vector<WiFiNetwork> networks;
    
    Serial.println("Scanning WiFi networks...");
    int n = WiFi.scanNetworks();
    
    for (int i = 0; i < n; i++) {
        WiFiNetwork network;
        network.ssid = WiFi.SSID(i);
        network.rssi = WiFi.RSSI(i);
        network.secured = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        
        bool duplicate = false;
        for (const auto& existing : networks) {
            if (existing.ssid == network.ssid) {
                duplicate = true;
                break;
            }
        }
        
        if (!duplicate && network.ssid.length() > 0) {
            networks.push_back(network);
        }
    }
    
    std::sort(networks.begin(), networks.end(), 
              [](const WiFiNetwork& a, const WiFiNetwork& b) {
                  return a.rssi > b.rssi;
              });
    
    WiFi.scanDelete();
    return networks;
}

bool WiFiConfigServer::connectToWiFi(const String& ssid, const String& password, unsigned long timeout) {
    Serial.printf("Connecting to WiFi: %s\n", ssid.c_str());
    
    // Keep AP+STA mode to allow configuration access
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("WiFi Config still accessible at: http://192.168.4.1:8080\n");
        Serial.printf("Or via station IP: http://%s:8080\n", WiFi.localIP().toString().c_str());
        // Keep AP mode active for future configuration
        return true;
    } else {
        Serial.println("\nConnection failed!");
        return false;
    }
}

void WiFiConfigServer::disconnectWiFi() {
    WiFi.disconnect();
    Serial.println("WiFi disconnected");
}

bool WiFiConfigServer::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiConfigServer::getConnectedSSID() {
    return WiFi.SSID();
}

String WiFiConfigServer::getLocalIP() {
    return WiFi.localIP().toString();
}

int WiFiConfigServer::getRSSI() {
    return WiFi.RSSI();
}

void WiFiConfigServer::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected\n", client->id());
        sendWiFiStatus();
        sendWiFiList();
        sendSensorData();
        sendLEDStatus();
        sendLightSensorData();
        
        // Send alert settings
        DynamicJsonDocument doc(512);
        doc["type"] = "alert_settings";
        doc["alert_r"] = alertNeoR;
        doc["alert_g"] = alertNeoG;
        doc["alert_b"] = alertNeoB;
        doc["alert_hex"] = alertNeoHex;
        doc["temp_threshold"] = tempThreshold;
        doc["current_temp"] = glob_temperature;
        doc["temp_alert"] = glob_temp_alert;
        
        String message;
        serializeJson(doc, message);
        ws->textAll(message);
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        handleWebSocketMessage(arg, data, len);
    }
}

void WiFiConfigServer::handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    
    if (info->opcode == WS_TEXT) {
        String message;
        message += String((char*)data).substring(0, len);
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, message);
        
        if (error) {
            Serial.println("JSON parsing failed");
            return;
        }
        
        String action = doc["action"];
        
        if (action == "scan") {
            sendWiFiList();
        } else if (action == "connect") {
            String ssid = doc["ssid"];
            String password = doc["password"];
            
            if (connectToWiFi(ssid, password)) {
                saveWiFiCredentials(ssid, password);
                DynamicJsonDocument response(256);
                response["type"] = "connect_result";
                response["success"] = true;
                response["message"] = "Connected successfully";
                
                String responseStr;
                serializeJson(response, responseStr);
                ws->textAll(responseStr);
            } else {
                DynamicJsonDocument response(256);
                response["type"] = "connect_result";
                response["success"] = false;
                response["message"] = "Connection failed";
                
                String responseStr;
                serializeJson(response, responseStr);
                ws->textAll(responseStr);
            }
        } else if (action == "disconnect") {
            disconnectWiFi();
            sendWiFiStatus();
        } else if (action == "get_status") {
            sendWiFiStatus();
        } else if (action == "get_sensors") {
            sendSensorData();
        } else if (action == "get_leds") {
            sendLEDStatus();
        } else if (action == "control_led") {
            bool state = doc["state"];
            setLEDState(state);
            sendLEDStatus();
        // Manual NeoPixel control functions restored for normal operation
        } else if (action == "control_neo") {
            bool state = doc["state"];
            setNeoState(state);
            sendLEDStatus();
        } else if (action == "preview_neo_color") {
            uint8_t r = doc["r"];
            uint8_t g = doc["g"];
            uint8_t b = doc["b"];
            setNeoColor(r, g, b);
            
            // Send confirmation response for preview
            DynamicJsonDocument response(256);
            response["type"] = "neo_color_result";
            response["action"] = "preview";
            response["success"] = true;
            String responseStr;
            serializeJson(response, responseStr);
            ws->textAll(responseStr);
        } else if (action == "save_neo_color") {
            uint8_t r = doc["r"];
            uint8_t g = doc["g"];
            uint8_t b = doc["b"];
            String hex = doc["hex"];
            
            bool saved = saveNeoColor(r, g, b, hex);
            if (!isBlinking) { // Apply immediately if not in alert mode
                setNeoColor(r, g, b);
            }
            
            // Send confirmation response for save
            DynamicJsonDocument response(256);
            response["type"] = "neo_color_result";
            response["action"] = "save";
            response["success"] = saved;
            String responseStr;
            serializeJson(response, responseStr);
            ws->textAll(responseStr);
        } else if (action == "get_light") {
            sendLightSensorData();
        } else if (action == "save_alert_color") {
            uint8_t r = doc["r"];
            uint8_t g = doc["g"];
            uint8_t b = doc["b"];
            String hex = doc["hex"];
            
            bool saved = saveAlertColor(r, g, b, hex);
            
            DynamicJsonDocument response(256);
            response["type"] = "alert_color_result";
            response["success"] = saved;
            String responseStr;
            serializeJson(response, responseStr);
            ws->textAll(responseStr);
        } else if (action == "save_temp_threshold") {
            float threshold = doc["threshold"];
            
            bool saved = saveTempThreshold(threshold);
            
            DynamicJsonDocument response(256);
            response["type"] = "temp_threshold_result";
            response["success"] = saved;
            response["threshold"] = threshold;
            String responseStr;
            serializeJson(response, responseStr);
            ws->textAll(responseStr);
        } else if (action == "get_alert_settings") {
            DynamicJsonDocument response(512);
            response["type"] = "alert_settings";
            response["alert_r"] = alertNeoR;
            response["alert_g"] = alertNeoG;
            response["alert_b"] = alertNeoB;
            response["alert_hex"] = alertNeoHex;
            response["temp_threshold"] = tempThreshold;
            response["current_temp"] = glob_temperature;
            response["temp_alert"] = glob_temp_alert;
            
            String responseStr;
            serializeJson(response, responseStr);
            ws->textAll(responseStr);
        }
    }
}

void WiFiConfigServer::setupConfigRoutes() {
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // Serve dashboard.html as root page
        if (LittleFS.exists("/dashboard.html")) {
            request->send(LittleFS, "/dashboard.html", "text/html");
        } else {
            request->send(404, "text/html", "<h1>Dashboard not found</h1><p>Please upload dashboard.html to LittleFS</p>");
        }
    });
    
    server->on("/wifi-config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // Try to serve from LittleFS first, fallback to function
        if (LittleFS.exists("/wifi_config.html")) {
            request->send(LittleFS, "/wifi_config.html", "text/html");
        } else {
            request->send(200, "text/html", getConfigPageHTML());
        }
    });
    
    server->on("/dashboard", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/dashboard.html")) {
            request->send(LittleFS, "/dashboard.html", "text/html");
        } else {
            request->send(404, "text/html", "<h1>Dashboard not found</h1><p>Please upload dashboard.html to LittleFS</p>");
        }
    });
    
    server->on("/scan", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getWiFiScanJSON());
    });
    
    server->on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getWiFiStatusJSON());
    });
    
    server->on("/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getSensorDataJSON());
    });
    
    server->on("/leds", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getLEDStatusJSON());
    });
    
    server->on("/light", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getLightSensorJSON());
    });
    
    server->on("/alert", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getAlertSettingsJSON());
    });
}

String WiFiConfigServer::getWiFiScanJSON() {
    auto networks = scanWiFiNetworks();
    
    DynamicJsonDocument doc(2048);
    JsonArray array = doc.createNestedArray("networks");
    
    for (const auto& network : networks) {
        JsonObject obj = array.createNestedObject();
        obj["ssid"] = network.ssid;
        obj["rssi"] = network.rssi;
        obj["secured"] = network.secured;
        obj["strength"] = (network.rssi + 100) * 2;
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

String WiFiConfigServer::getWiFiStatusJSON() {
    DynamicJsonDocument doc(512);
    
    doc["connected"] = isConnected();
    doc["ssid"] = getConnectedSSID();
    doc["ip"] = getLocalIP();
    doc["rssi"] = getRSSI();
    doc["config_mode"] = isConfigMode;
    doc["config_ssid"] = configSSID;
    doc["config_ip"] = isConfigMode ? WiFi.softAPIP().toString() : "";
    
    String result;
    serializeJson(doc, result);
    return result;
}

void WiFiConfigServer::sendWiFiStatus() {
    if (ws->count() > 0) {
        DynamicJsonDocument doc(512);
        doc["type"] = "status";
        doc["connected"] = isConnected();
        doc["ssid"] = getConnectedSSID();
        doc["ip"] = getLocalIP();
        doc["rssi"] = getRSSI();
        doc["config_mode"] = isConfigMode;
        
        String message;
        serializeJson(doc, message);
        ws->textAll(message);
    }
}

void WiFiConfigServer::sendWiFiList() {
    if (ws->count() > 0) {
        auto networks = scanWiFiNetworks();
        
        DynamicJsonDocument doc(2048);
        doc["type"] = "networks";
        JsonArray array = doc.createNestedArray("list");
        
        for (const auto& network : networks) {
            JsonObject obj = array.createNestedObject();
            obj["ssid"] = network.ssid;
            obj["rssi"] = network.rssi;
            obj["secured"] = network.secured;
            obj["strength"] = (network.rssi + 100) * 2;
        }
        
        String message;
        serializeJson(doc, message);
        ws->textAll(message);
    }
}

void WiFiConfigServer::broadcastMessage(const String& message) {
    if (ws->count() > 0) {
        ws->textAll(message);
    }
}

String WiFiConfigServer::getSensorDataJSON() {
    DynamicJsonDocument doc(512);
    
    doc["temperature"] = glob_temperature;
    doc["humidity"] = glob_humidity;
    doc["light_level"] = glob_light_level;
    doc["led_state"] = glob_led_state;
    doc["temp_alert"] = glob_temp_alert;
    doc["temp_threshold"] = tempThreshold;
    doc["timestamp"] = millis();
    doc["valid"] = !isnan(glob_temperature) && !isnan(glob_humidity);
    
    String result;
    serializeJson(doc, result);
    return result;
}

void WiFiConfigServer::sendSensorData() {
    if (ws->count() > 0) {
        DynamicJsonDocument doc(512);
        doc["type"] = "sensors";
        doc["temperature"] = glob_temperature;
        doc["humidity"] = glob_humidity;
        doc["light_level"] = glob_light_level;
        doc["led_state"] = glob_led_state;
        doc["temp_alert"] = glob_temp_alert;
        doc["temp_threshold"] = tempThreshold;
        doc["timestamp"] = millis();
        doc["valid"] = !isnan(glob_temperature) && !isnan(glob_humidity);
        
        String message;
        serializeJson(doc, message);
        ws->textAll(message);
    }
}

String WiFiConfigServer::getLEDStatusJSON() {
    DynamicJsonDocument doc(512);
    
    doc["led_state"] = ledState;
    doc["neo_state"] = neoState;
    doc["light_led_state"] = glob_led_state;
    doc["led_pin"] = LED_GPIO;
    doc["neo_pin"] = NEO_PIN;
    doc["light_led_pin"] = 2;
    doc["timestamp"] = millis();
    
    String result;
    serializeJson(doc, result);
    return result;
}

String WiFiConfigServer::getLightSensorJSON() {
    DynamicJsonDocument doc(256);
    
    doc["light_level"] = glob_light_level;
    doc["led_state"] = glob_led_state;
    doc["threshold"] = 500;
    doc["sensor_pin"] = 1;
    doc["led_pin"] = 2;
    doc["timestamp"] = millis();
    
    String result;
    serializeJson(doc, result);
    return result;
}

void WiFiConfigServer::sendLEDStatus() {
    if (ws->count() > 0) {
        DynamicJsonDocument doc(256);
        doc["type"] = "leds";
        doc["led_state"] = ledState;
        doc["neo_state"] = neoState;
        doc["led_pin"] = LED_GPIO;
        doc["neo_pin"] = NEO_PIN;
        doc["timestamp"] = millis();
        
        String message;
        serializeJson(doc, message);
        ws->textAll(message);
    }
}

void WiFiConfigServer::sendLightSensorData() {
    if (ws->count() > 0) {
        DynamicJsonDocument doc(256);
        doc["type"] = "light";
        doc["light_level"] = glob_light_level;
        doc["led_state"] = glob_led_state;
        doc["threshold"] = 500;
        doc["sensor_pin"] = 1;
        doc["led_pin"] = 2;
        doc["timestamp"] = millis();
        
        String message;
        serializeJson(doc, message);
        ws->textAll(message);
    }
}

void WiFiConfigServer::setLEDState(bool state) {
    ledState = state;
    digitalWrite(LED_GPIO, state ? HIGH : LOW);
    Serial.printf("LED GPIO %d set to %s\n", LED_GPIO, state ? "ON" : "OFF");
}

// Manual control disabled for temperature-based automatic control
/*
void WiFiConfigServer::setNeoState(bool state) {
    neoState = state;
    if (state) {
        // Use saved color when turning on
        neoPixel->setPixelColor(0, neoPixel->Color(savedNeoR, savedNeoG, savedNeoB));
    } else {
        neoPixel->setPixelColor(0, neoPixel->Color(0, 0, 0)); // Off
    }
    neoPixel->show();
    Serial.printf("NeoPixel GPIO %d set to %s (saved color: RGB(%d,%d,%d))\n", 
                  NEO_PIN, state ? "ON" : "OFF", savedNeoR, savedNeoG, savedNeoB);
}
*/

// Temperature-based NeoPixel control function
void WiFiConfigServer::setNeoColorForTemperature(float temperature) {
    if (temperature > tempThreshold) {
        // Start blinking with alert color when temperature is above threshold
        if (!isBlinking) {
            isBlinking = true;
            lastBlinkTime = millis();
            blinkState = true;
        }
        neoState = true;
        glob_temp_alert = true;
        Serial.printf("NeoPixel GPIO %d blinking alert color RGB(%d,%d,%d) due to high temperature: %.2f°C (threshold: %.1f°C)\n", 
                      NEO_PIN, alertNeoR, alertNeoG, alertNeoB, temperature, tempThreshold);
    } else {
        // Return to normal color when temperature is at or below threshold
        isBlinking = false;
        neoPixel->setPixelColor(0, neoPixel->Color(savedNeoR, savedNeoG, savedNeoB));
        neoPixel->show();
        neoState = true;
        glob_temp_alert = false;
        Serial.printf("NeoPixel GPIO %d set to normal color RGB(%d,%d,%d), temperature: %.2f°C (threshold: %.1f°C)\n", 
                      NEO_PIN, savedNeoR, savedNeoG, savedNeoB, temperature, tempThreshold);
    }
}

// Handle NeoPixel blinking during temperature alerts
void WiFiConfigServer::handleNeoBlinking() {
    if (isBlinking) {
        unsigned long currentTime = millis();
        if (currentTime - lastBlinkTime > 500) { // Blink every 500ms
            blinkState = !blinkState;
            lastBlinkTime = currentTime;
            
            if (blinkState) {
                // Show alert color
                neoPixel->setPixelColor(0, neoPixel->Color(alertNeoR, alertNeoG, alertNeoB));
            } else {
                // Turn off
                neoPixel->setPixelColor(0, neoPixel->Color(0, 0, 0));
            }
            neoPixel->show();
        }
    }
}

// Manual color setting for normal operation
void WiFiConfigServer::setNeoColor(uint8_t r, uint8_t g, uint8_t b) {
    if (!isBlinking) { // Only allow manual control when not in alert mode
        neoPixel->setPixelColor(0, neoPixel->Color(r, g, b));
        neoPixel->show();
        neoState = true;
        Serial.printf("NeoPixel GPIO %d color set to RGB(%d, %d, %d)\n", NEO_PIN, r, g, b);
    }
}

// Manual state control for normal operation
void WiFiConfigServer::setNeoState(bool state) {
    if (!isBlinking) { // Only allow manual control when not in alert mode
        neoState = state;
        if (state) {
            neoPixel->setPixelColor(0, neoPixel->Color(savedNeoR, savedNeoG, savedNeoB));
        } else {
            neoPixel->setPixelColor(0, neoPixel->Color(0, 0, 0));
        }
        neoPixel->show();
        Serial.printf("NeoPixel GPIO %d set to %s\n", NEO_PIN, state ? "ON" : "OFF");
    }
}

bool WiFiConfigServer::saveNeoColor(uint8_t r, uint8_t g, uint8_t b, const String& hex) {
    savedNeoR = r;
    savedNeoG = g;
    savedNeoB = b;
    savedNeoHex = hex;
    
    // Save to preferences
    preferences.putUChar("neo_r", r);
    preferences.putUChar("neo_g", g);
    preferences.putUChar("neo_b", b);
    preferences.putString("neo_hex", hex);
    
    Serial.printf("NeoPixel color saved: RGB(%d, %d, %d) = %s\n", r, g, b, hex.c_str());
    return true;
}

void WiFiConfigServer::loadSavedNeoColor() {
    // Load saved color from preferences, use default if not found
    savedNeoR = preferences.getUChar("neo_r", 0);
    savedNeoG = preferences.getUChar("neo_g", 255);
    savedNeoB = preferences.getUChar("neo_b", 0);
    savedNeoHex = preferences.getString("neo_hex", "#00ff00");
    
    Serial.printf("Loaded saved NeoPixel color: RGB(%d, %d, %d) = %s\n", 
                  savedNeoR, savedNeoG, savedNeoB, savedNeoHex.c_str());
    
    // Send saved color to connected clients
    DynamicJsonDocument doc(256);
    doc["type"] = "saved_color";
    doc["r"] = savedNeoR;
    doc["g"] = savedNeoG;
    doc["b"] = savedNeoB;
    doc["hex"] = savedNeoHex;
    
    String message;
    serializeJson(doc, message);
    ws->textAll(message);
}

bool WiFiConfigServer::getLEDState() {
    return ledState;
}

bool WiFiConfigServer::getNeoState() {
    return neoState;
}

bool WiFiConfigServer::saveAlertColor(uint8_t r, uint8_t g, uint8_t b, const String& hex) {
    alertNeoR = r;
    alertNeoG = g;
    alertNeoB = b;
    alertNeoHex = hex;
    
    // Save to preferences
    preferences.putUChar("alert_r", r);
    preferences.putUChar("alert_g", g);
    preferences.putUChar("alert_b", b);
    preferences.putString("alert_hex", hex);
    
    Serial.printf("Alert color saved: RGB(%d, %d, %d) = %s\n", r, g, b, hex.c_str());
    return true;
}

bool WiFiConfigServer::saveTempThreshold(float threshold) {
    tempThreshold = threshold;
    HIGH_TEMP_THRESHOLD = threshold; // Update global threshold
    
    // Save to preferences
    preferences.putFloat("temp_threshold", threshold);
    
    Serial.printf("Temperature threshold saved: %.1f°C\n", threshold);
    return true;
}

void WiFiConfigServer::loadAlertSettings() {
    // Load saved alert color from preferences, use default if not found
    alertNeoR = preferences.getUChar("alert_r", 255);
    alertNeoG = preferences.getUChar("alert_g", 0);
    alertNeoB = preferences.getUChar("alert_b", 0);
    alertNeoHex = preferences.getString("alert_hex", "#ff0000");
    
    // Load temperature threshold
    tempThreshold = preferences.getFloat("temp_threshold", 30.0);
    HIGH_TEMP_THRESHOLD = tempThreshold; // Update global threshold
    
    Serial.printf("Loaded alert settings: Color RGB(%d, %d, %d) = %s, Threshold: %.1f°C\n", 
                  alertNeoR, alertNeoG, alertNeoB, alertNeoHex.c_str(), tempThreshold);
}

String WiFiConfigServer::getAlertSettingsJSON() {
    DynamicJsonDocument doc(256);
    
    doc["alert_r"] = alertNeoR;
    doc["alert_g"] = alertNeoG;
    doc["alert_b"] = alertNeoB;
    doc["alert_hex"] = alertNeoHex;
    doc["temp_threshold"] = tempThreshold;
    doc["current_temp"] = glob_temperature;
    doc["temp_alert"] = glob_temp_alert;
    doc["timestamp"] = millis();
    
    String result;
    serializeJson(doc, result);
    return result;
}

String WiFiConfigServer::getConfigPageHTML() {
    // Try to read HTML from LittleFS file
    if (LittleFS.exists("/wifi_config.html")) {
        File file = LittleFS.open("/wifi_config.html", "r");
        if (file) {
            String html = file.readString();
            file.close();
            Serial.println("HTML loaded from /wifi_config.html");
            return html;
        } else {
            Serial.println("Failed to open /wifi_config.html");
        }
    } else {
        Serial.println("/wifi_config.html not found in LittleFS");
    }
    
    // Return simple error message if file not found
    Serial.println("WiFi config HTML file not found");
    return "<html><body><h1>WiFi Configuration</h1><p>Configuration file not found. Please upload wifi_config.html to LittleFS.</p></body></html>";
}