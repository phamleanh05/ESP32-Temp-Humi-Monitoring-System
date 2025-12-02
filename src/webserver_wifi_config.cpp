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
    : server(webServer), ws(webSocket), isConfigMode(false), ledState(false), neoState(false) {
    // Initialize LED pins
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LOW);
    
    // Initialize NeoPixel
    neoPixel = new Adafruit_NeoPixel(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    neoPixel->begin();
    neoPixel->clear();
    neoPixel->show();
}

void WiFiConfigServer::begin() {
    // LittleFS initialization is handled by check_info_File in main
    
    ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWsEvent(server, client, type, arg, data, len);
    });
    
    server->addHandler(ws);
    setupConfigRoutes();
    
    // Start the web server
    server->begin();
    Serial.println("WiFi Config Web Server started");
    
    // Check if WiFi credentials exist in info.dat file
    if (check_info_File(true)) {
        if (!connectToWiFi(WIFI_SSID, WIFI_PASS)) {
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
        lastSensorUpdate = millis();
    }
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
    // Use task_check_info function to save WiFi credentials
    // Preserve existing IoT settings when saving WiFi credentials
    Save_info_File(ssid, password, CORE_IOT_TOKEN.isEmpty() ? "" : CORE_IOT_TOKEN, 
                   CORE_IOT_SERVER.isEmpty() ? "" : CORE_IOT_SERVER, 
                   CORE_IOT_PORT.isEmpty() ? "" : CORE_IOT_PORT);
    Serial.printf("WiFi credentials saved: %s\n", ssid.c_str());
    return true;
}

WiFiCredentials WiFiConfigServer::loadWiFiCredentials() {
    WiFiCredentials creds;
    // Load credentials from global variables set by task_check_info
    creds.ssid = WIFI_SSID;
    creds.password = WIFI_PASS;
    return creds;
}

void WiFiConfigServer::clearWiFiCredentials() {
    // Use task_check_info function to delete credentials
    Delete_info_File();
    Serial.println("WiFi credentials cleared");
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
        } else if (action == "clear_credentials") {
            clearWiFiCredentials();
            DynamicJsonDocument response(256);
            response["type"] = "credentials_cleared";
            response["success"] = true;
            
            String responseStr;
            serializeJson(response, responseStr);
            ws->textAll(responseStr);
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
        } else if (action == "control_neo") {
            bool state = doc["state"];
            setNeoState(state);
            sendLEDStatus();
        } else if (action == "get_light") {
            sendLightSensorData();
        }
    }
}

void WiFiConfigServer::setupConfigRoutes() {
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // Try to serve from LittleFS first, fallback to function
        if (LittleFS.exists("/wifi_config.html")) {
            request->send(LittleFS, "/wifi_config.html", "text/html");
        } else {
            request->send(200, "text/html", getConfigPageHTML());
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

void WiFiConfigServer::setNeoState(bool state) {
    neoState = state;
    if (state) {
        neoPixel->setPixelColor(0, neoPixel->Color(0, 255, 0)); // Green when ON
    } else {
        neoPixel->setPixelColor(0, neoPixel->Color(0, 0, 0)); // Off
    }
    neoPixel->show();
    Serial.printf("NeoPixel GPIO %d set to %s\n", NEO_PIN, state ? "ON" : "OFF");
}

bool WiFiConfigServer::getLEDState() {
    return ledState;
}

bool WiFiConfigServer::getNeoState() {
    return neoState;
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