#include "wifi_config.h"

WiFiConfigServer* wifiConfig = nullptr;

// WiFi config server instances
AsyncWebServer wifiConfigServer(8080);
AsyncWebSocket wifiConfigWS("/ws");

// WiFi configuration task
void wifi_config_task(void *parameter)
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
    preferences.begin("wifi-config", false);
    
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    
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
    if (millis() - lastStatusUpdate > 5000) {
        sendWiFiStatus();
        lastStatusUpdate = millis();
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
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    Serial.printf("WiFi credentials saved: %s\n", ssid.c_str());
    return true;
}

WiFiCredentials WiFiConfigServer::loadWiFiCredentials() {
    WiFiCredentials creds;
    creds.ssid = preferences.getString("ssid", "");
    creds.password = preferences.getString("password", "");
    return creds;
}

void WiFiConfigServer::clearWiFiCredentials() {
    preferences.remove("ssid");
    preferences.remove("password");
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
    DynamicJsonDocument doc(256);
    
    doc["temperature"] = glob_temperature;
    doc["humidity"] = glob_humidity;
    doc["timestamp"] = millis();
    doc["valid"] = !isnan(glob_temperature) && !isnan(glob_humidity);
    
    String result;
    serializeJson(doc, result);
    return result;
}

void WiFiConfigServer::sendSensorData() {
    if (ws->count() > 0) {
        DynamicJsonDocument doc(256);
        doc["type"] = "sensors";
        doc["temperature"] = glob_temperature;
        doc["humidity"] = glob_humidity;
        doc["timestamp"] = millis();
        doc["valid"] = !isnan(glob_temperature) && !isnan(glob_humidity);
        
        String message;
        serializeJson(doc, message);
        ws->textAll(message);
    }
}

String WiFiConfigServer::getLEDStatusJSON() {
    DynamicJsonDocument doc(256);
    
    doc["led_state"] = ledState;
    doc["neo_state"] = neoState;
    doc["led_pin"] = LED_GPIO;
    doc["neo_pin"] = NEO_PIN;
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
    
    // Fallback: Return full HTML if file not found (temporary solution)
    Serial.println("Using fallback HTML with sensor support");
    return R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 WiFi Configuration</title>
<style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px;color:#333}.container{max-width:500px;margin:0 auto;background:white;border-radius:15px;box-shadow:0 15px 35px rgba(0,0,0,0.1);overflow:hidden}.header{background:linear-gradient(135deg,#4CAF50,#45a049);color:white;padding:25px;text-align:center}.content{padding:25px}.status{padding:15px;border-radius:8px;margin-bottom:20px;border-left:4px solid}.status.connected{background:#e8f5e8;border-color:#4CAF50;color:#2e7d32}.status.disconnected{background:#ffebee;border-color:#f44336;color:#c62828}.status.config{background:#fff3e0;border-color:#ff9800;color:#e65100}.network-list{margin:20px 0}.network-item{display:flex;justify-content:space-between;align-items:center;padding:15px;margin:8px 0;border-radius:8px;background:#f8f9fa;border:1px solid #e9ecef;cursor:pointer;transition:all 0.3s}.network-item:hover{background:#e9ecef;transform:translateY(-1px)}.network-info{flex:1}.network-name{font-weight:600;margin-bottom:4px}.network-details{font-size:12px;color:#666}.signal-strength{width:30px;height:20px;position:relative;margin-left:10px}.signal-bar{position:absolute;bottom:0;width:4px;background:#ddd;border-radius:1px}.signal-bar.active{background:#4CAF50}.signal-bar:nth-child(1){left:0;height:25%}.signal-bar:nth-child(2){left:6px;height:50%}.signal-bar:nth-child(3){left:12px;height:75%}.signal-bar:nth-child(4){left:18px;height:100%}input[type="password"]{width:100%;padding:12px;margin:10px 0;border:1px solid #ddd;border-radius:8px;font-size:16px}.btn{padding:12px 24px;border:none;border-radius:8px;cursor:pointer;font-size:16px;transition:all 0.3s;margin:5px}.btn-primary{background:#4CAF50;color:white}.btn-secondary{background:#6c757d;color:white}.btn-danger{background:#dc3545;color:white}.btn:hover{transform:translateY(-1px);box-shadow:0 4px 8px rgba(0,0,0,0.2)}.btn:disabled{opacity:0.5;cursor:not-allowed}.hidden{display:none}.loading{text-align:center;padding:20px;color:#666}.modal{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.5);display:none;align-items:center;justify-content:center;z-index:1000}.modal-content{background:white;padding:30px;border-radius:15px;width:90%;max-width:400px;text-align:center}.controls{text-align:center;margin:20px 0}</style></head>
<body><div class="container"><div class="header"><h1>🌐 WiFi Configuration</h1><p>ESP32 Network Setup</p></div>
<div class="content"><div id="status" class="status disconnected"><strong>Status:</strong> <span id="status-text">Disconnected</span><div id="status-details"></div></div>
<div id="sensors" class="status" style="background:#e3f2fd;border-color:#2196F3;color:#0d47a1;"><strong>🌡️ Sensors:</strong> <span id="temperature">--°C</span> | <span id="humidity">--%</span><div id="sensor-details" style="font-size:12px;margin-top:5px;">Last update: --</div></div><div id="led-controls" class="status" style="background:#fff3e0;border-color:#ff9800;color:#e65100;"><strong>💡 LED Control:</strong><div style="margin-top:10px;"><button id="led-btn" class="btn btn-secondary" onclick="toggleLED()">💡 LED (GPIO 48): <span id="led-status">OFF</span></button> <button id="neo-btn" class="btn btn-secondary" onclick="toggleNeo()">🌈 NeoPixel (GPIO 45): <span id="neo-status">OFF</span></button></div></div>
<div class="controls"><button class="btn btn-primary" onclick="scanNetworks()">🔍 Scan Networks</button><button class="btn btn-secondary" onclick="refreshStatus()">🔄 Refresh</button><button class="btn btn-danger" onclick="disconnect()">❌ Disconnect</button><button class="btn btn-danger" onclick="clearCredentials()">🗑️ Clear Saved</button></div>
<div id="loading" class="loading hidden"><p>📡 Scanning for networks...</p></div><div id="network-list" class="network-list"></div></div></div>
<div id="passwordModal" class="modal"><div class="modal-content"><h3>🔐 Enter Password</h3><p>Network: <strong id="selected-ssid"></strong></p><input type="password" id="password-input" placeholder="Enter WiFi password"><div style="margin-top:20px;"><button class="btn btn-primary" onclick="connectToNetwork()">Connect</button><button class="btn btn-secondary" onclick="closeModal()">Cancel</button></div></div></div>
<script>let ws;let selectedSSID='';function initWebSocket(){const protocol=location.protocol==='https:' ? 'wss:' : 'ws:';ws=new WebSocket(`${protocol}//${location.host}/ws`);ws.onopen=function(){console.log('WebSocket connected');refreshStatus();refreshSensors();refreshLEDs();};ws.onmessage=function(event){const data=JSON.parse(event.data);handleWebSocketMessage(data);};ws.onclose=function(){console.log('WebSocket disconnected, reconnecting...');setTimeout(initWebSocket,3000);};}function handleWebSocketMessage(data){if(data.type==='status'){updateStatus(data);}else if(data.type==='networks'){updateNetworkList(data.list);}else if(data.type==='sensors'){updateSensorData(data);}else if(data.type==='leds'){updateLEDStatus(data);}else if(data.type==='connect_result'){if(data.success){alert('✅ Connected successfully!');closeModal();refreshStatus();}else{alert('❌ Connection failed: '+data.message);}}else if(data.type==='credentials_cleared'){alert('🗑️ Saved credentials cleared');refreshStatus();}}function updateStatus(status){const statusEl=document.getElementById('status');const statusTextEl=document.getElementById('status-text');const statusDetailsEl=document.getElementById('status-details');if(status.connected){statusEl.className='status connected';statusTextEl.textContent=`Connected to ${status.ssid}`;statusDetailsEl.innerHTML=`IP: ${status.ip} | Signal: ${status.rssi} dBm`;}else if(status.config_mode){statusEl.className='status config';statusTextEl.textContent='Configuration Mode';statusDetailsEl.innerHTML='Connect to this device to configure WiFi';}else{statusEl.className='status disconnected';statusTextEl.textContent='Disconnected';statusDetailsEl.innerHTML='Not connected to any network';}}function updateSensorData(data){const tempEl=document.getElementById('temperature');const humidityEl=document.getElementById('humidity');const sensorDetailsEl=document.getElementById('sensor-details');const sensorsEl=document.getElementById('sensors');if(data.valid){tempEl.textContent=`${data.temperature.toFixed(1)}°C`;humidityEl.textContent=`${data.humidity.toFixed(1)}%`;sensorsEl.style.background='#e8f5e8';sensorsEl.style.borderColor='#4CAF50';sensorsEl.style.color='#2e7d32';}else{tempEl.textContent='Error';humidityEl.textContent='Error';sensorsEl.style.background='#ffebee';sensorsEl.style.borderColor='#f44336';sensorsEl.style.color='#c62828';}const now=new Date();sensorDetailsEl.textContent=`Last update: ${now.toLocaleTimeString()}`;}function updateNetworkList(networks){const listEl=document.getElementById('network-list');const loadingEl=document.getElementById('loading');loadingEl.classList.add('hidden');if(networks.length===0){listEl.innerHTML='<p style="text-align: center; color: #666;">No networks found</p>';return;}listEl.innerHTML=networks.map(network => {const signalBars=Math.ceil(network.strength / 25);const lockIcon=network.secured ? '🔒' : '🔓';return `<div class="network-item" onclick="selectNetwork('${network.ssid}', ${network.secured})"><div class="network-info"><div class="network-name">${lockIcon} ${network.ssid}</div><div class="network-details">${network.rssi} dBm | ${network.secured ? 'Secured' : 'Open'}</div></div><div class="signal-strength">${[1,2,3,4].map(i => `<div class="signal-bar ${i <= signalBars ? 'active' : ''}"></div>`).join('')}</div></div>`;}).join('');}function scanNetworks(){document.getElementById('loading').classList.remove('hidden');document.getElementById('network-list').innerHTML='';if(ws && ws.readyState===WebSocket.OPEN){ws.send(JSON.stringify({action:'scan'}));}}function selectNetwork(ssid,secured){selectedSSID=ssid;document.getElementById('selected-ssid').textContent=ssid;if(secured){document.getElementById('passwordModal').style.display='flex';document.getElementById('password-input').value='';document.getElementById('password-input').focus();}else{connectToNetwork();}}function connectToNetwork(){const password=document.getElementById('password-input').value;if(ws && ws.readyState===WebSocket.OPEN){ws.send(JSON.stringify({action:'connect',ssid:selectedSSID,password:password}));}}function disconnect(){if(ws && ws.readyState===WebSocket.OPEN){ws.send(JSON.stringify({action:'disconnect'}));}}function clearCredentials(){if(confirm('Clear saved WiFi credentials?')){if(ws && ws.readyState===WebSocket.OPEN){ws.send(JSON.stringify({action:'clear_credentials'}));}}}function refreshStatus(){if(ws && ws.readyState===WebSocket.OPEN){ws.send(JSON.stringify({action:'get_status'}));ws.send(JSON.stringify({action:'get_sensors'}));}}function refreshSensors(){if(ws && ws.readyState===WebSocket.OPEN){ws.send(JSON.stringify({action:'get_sensors'}));}}function refreshLEDs(){if(ws && ws.readyState===WebSocket.OPEN){ws.send(JSON.stringify({action:'get_leds'}));}}function updateLEDStatus(data){const ledStatusEl=document.getElementById('led-status');const neoStatusEl=document.getElementById('neo-status');const ledBtnEl=document.getElementById('led-btn');const neoBtnEl=document.getElementById('neo-btn');ledStatusEl.textContent=data.led_state?'ON':'OFF';ledBtnEl.className=data.led_state?'btn btn-primary':'btn btn-secondary';neoStatusEl.textContent=data.neo_state?'ON':'OFF';neoBtnEl.className=data.neo_state?'btn btn-primary':'btn btn-secondary';}function toggleLED(){if(ws && ws.readyState===WebSocket.OPEN){const currentState=document.getElementById('led-status').textContent==='ON';ws.send(JSON.stringify({action:'control_led',state:!currentState}));}}function toggleNeo(){if(ws && ws.readyState===WebSocket.OPEN){const currentState=document.getElementById('neo-status').textContent==='ON';ws.send(JSON.stringify({action:'control_neo',state:!currentState}));}}function closeModal(){document.getElementById('passwordModal').style.display='none';}document.getElementById('password-input').addEventListener('keypress',function(e){if(e.key==='Enter'){connectToNetwork();}});window.onclick=function(event){const modal=document.getElementById('passwordModal');if(event.target===modal){closeModal();}};initWebSocket();</script></body></html>)rawliteral";
}