#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <EEPROM.h>

const char* ssid = "MAGTI-1";
const char* password = "gorgadze12";

#define DHTPIN D1
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const int relayPin1 = D2;        // პირველი რელე (გათბობა)
const int relayPin2 = D3;        // მეორე რელე (მორწყვა)
const int soilMoisturePin = A0;  // ნიადაგის ტენიანობის სენსორი

bool relayState1 = true; 
bool relayState2 = true; 

float t = 0.0;
float h = 0.0;

const int AirValue = 667;   // მშრალი ჰაერის/მიწის კალიბრაციის მნიშვნელობა
const int WaterValue = 280; // წყლის კალიბრაციის მნიშვნელობა
int soilMoistureValue = 0;
int soilmoisturepercent = 0;

#define EEPROM_SIZE 2
#define RELAY1_ADDR 0
#define RELAY2_ADDR 1

void saveRelayState(int addr, bool state);

// სტატიკური IP კონფიგურაცია
IPAddress local_IP(192, 168, 1, 101);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

AsyncWebServer server(80);

const char* htmlPage = R"=====(
<!DOCTYPE html>
<html lang="ka">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>სათბურის მართვის პანელი</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <style>
        :root {
            --bg-color: #f4f7f6;
            --card-bg: #ffffff;
            --text-color: #2c3e50;
            --text-muted: #7f8c8d;
            --accent-green: #2ecc71;
            --accent-blue: #3498db;
            --accent-red: #e74c3c;
            --accent-brown: #e67e22;
            --border-radius: 16px;
            --shadow: 0 10px 25px rgba(0, 0, 0, 0.05);
        }

        body.dark-mode {
            --bg-color: #1a1a2e;
            --card-bg: #16213e;
            --text-color: #e9ecef;
            --text-muted: #a0aec0;
            --shadow: 0 10px 25px rgba(0, 0, 0, 0.3);
        }

        * { box-sizing: border-box; margin: 0; padding: 0; transition: background-color 0.3s, color 0.3s; }
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: var(--bg-color); color: var(--text-color); padding: 20px; min-height: 100vh; }
        
        .header { display: flex; justify-content: space-between; align-items: center; max-width: 900px; margin: 0 auto 30px auto; }
        .header h1 { font-size: 1.8rem; display: flex; align-items: center; gap: 10px; color: var(--accent-green); }

        /* Dark Mode Toggle */
        .theme-toggle { background: var(--card-bg); border: none; color: var(--text-color); padding: 10px 16px; border-radius: 30px; cursor: pointer; box-shadow: var(--shadow); display: flex; align-items: center; gap: 8px; font-weight: 600; }

        .dashboard { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 20px; max-width: 900px; margin: 0 auto; }

        .card { background: var(--card-bg); border-radius: var(--border-radius); padding: 24px; box-shadow: var(--shadow); display: flex; flex-direction: column; gap: 16px; }
        .card-header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid rgba(0,0,0,0.05); padding-bottom: 12px; }
        .card-title { font-size: 1.1rem; font-weight: 600; display: flex; align-items: center; gap: 8px; }

        .metrics-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; text-align: center; margin: 10px 0; }
        .metric-box { background: rgba(0,0,0,0.02); padding: 15px; border-radius: 12px; }
        .dark-mode .metric-box { background: rgba(255,255,255,0.03); }
        .metric-value { font-size: 1.8rem; font-weight: 700; margin-top: 5px; }
        
        .temp-color { color: var(--accent-red); }
        .humid-color { color: var(--accent-blue); }
        .soil-color { color: var(--accent-brown); }

        /* Controls & Sliders */
        .control-group { display: flex; flex-direction: column; gap: 8px; font-size: 0.9rem; color: var(--text-muted); }
        .range-slider { display: flex; align-items: center; gap: 10px; }
        .range-slider input { flex-grow: 1; accent-color: var(--accent-green); cursor: pointer; }
        .range-val { font-weight: 600; width: 45px; text-align: right; color: var(--text-color); }

        /* Relay Switch Area */
        .relay-control { display: flex; justify-content: space-between; align-items: center; margin-top: 10px; padding-top: 15px; border-top: 1px solid rgba(0,0,0,0.05); }
        .status-badge { padding: 6px 12px; border-radius: 20px; font-size: 0.85rem; font-weight: 600; }
        .status-badge.on { background: rgba(46, 204, 113, 0.15); color: var(--accent-green); }
        .status-badge.off { background: rgba(231, 76, 60, 0.15); color: var(--accent-red); }

        .switch { position: relative; display: inline-block; width: 52px; height: 28px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .3s; border-radius: 34px; }
        .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 4px; bottom: 4px; background-color: white; transition: .3s; border-radius: 50%; }
        input:checked + .slider { background-color: var(--accent-green); }
        input:checked + .slider:before { transform: translateX(24px); }
    </style>
</head>
<body>

    <div class="header">
        <h1><i class="fa-solid fa-leaf"></i> სათბურის მართვა</h1>
        <button class="theme-toggle" onclick="toggleDarkMode()">
            <i class="fa-solid fa-moon" id="themeIcon"></i>
            <span id="themeText">მუქი</span>
        </button>
    </div>

    <div class="dashboard">
        <!-- Climate Control Card -->
        <div class="card">
            <div class="card-header">
                <div class="card-title"><i class="fa-solid fa-temperature-three-quarters temp-color"></i> ჰაერის კლიმატი</div>
                <span id="heatingBadge" class="status-badge off">გათბობა: OFF</span>
            </div>
            
            <div class="metrics-grid">
                <div class="metric-box">
                    <div>ტემპერატურა</div>
                    <div id="temp" class="metric-value temp-color">--°C</div>
                </div>
                <div class="metric-box">
                    <div>ტენიანობა</div>
                    <div id="humid" class="metric-value humid-color">--%</div>
                </div>
            </div>

            <div class="control-group">
                <label>ქვედა ზღვარი (ჩართვა):</label>
                <div class="range-slider">
                    <input type="range" id="tempLowerLimit" min="0" max="50" value="18" oninput="updateLimitDisplay('temp')">
                    <span id="tempLowerLimitDisplay" class="range-val">18°C</span>
                </div>
            </div>

            <div class="control-group">
                <label>ზედა ზღვარი (გამორთვა):</label>
                <div class="range-slider">
                    <input type="range" id="tempLimit" min="0" max="50" value="28" oninput="updateLimitDisplay('temp')">
                    <span id="tempLimitDisplay" class="range-val">28°C</span>
                </div>
            </div>

            <div class="relay-control">
                <span>გათბობის ავტო/ხელით მართვა</span>
                <label class="switch">
                    <input type="checkbox" id="heatingToggle" onclick="toggleHeating()">
                    <span class="slider"></span>
                </label>
            </div>
        </div>

        <!-- Soil Moisture Card -->
        <div class="card">
            <div class="card-header">
                <div class="card-title"><i class="fa-solid fa-droplet soil-color"></i> ნიადაგის მორწყვა</div>
                <span id="wateringBadge" class="status-badge off">მორწყვა: OFF</span>
            </div>

            <div class="metrics-grid" style="grid-template-columns: 1fr;">
                <div class="metric-box">
                    <div>ნიადაგის ტენიანობა</div>
                    <div id="moisture" class="metric-value soil-color">--%</div>
                </div>
            </div>

            <div class="control-group">
                <label>ქვედა ზღვარი (ჩართვა):</label>
                <div class="range-slider">
                    <input type="range" id="moistureLowerLimit" min="0" max="100" value="30" oninput="updateLimitDisplay('moisture')">
                    <span id="moistureLowerLimitDisplay" class="range-val">30%</span>
                </div>
            </div>

            <div class="control-group">
                <label>ზედა ზღვარი (გამორთვა):</label>
                <div class="range-slider">
                    <input type="range" id="moistureLimit" min="0" max="100" value="70" oninput="updateLimitDisplay('moisture')">
                    <span id="moistureLimitDisplay" class="range-val">70%</span>
                </div>
            </div>

            <div class="relay-control">
                <span>მორწყვის ავტო/ხელით მართვა</span>
                <label class="switch">
                    <input type="checkbox" id="wateringToggle" onclick="toggleWatering()">
                    <span class="slider"></span>
                </label>
            </div>
        </div>
    </div>

    <script>
        function updateReadings() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (xhr.readyState == XMLHttpRequest.DONE && xhr.status == 200) {
                    var response = JSON.parse(xhr.responseText);
                    
                    document.getElementById("temp").innerText = response.temperature.toFixed(1) + '°C';
                    document.getElementById("humid").innerText = response.humidity.toFixed(1) + "%";
                    document.getElementById("moisture").innerText = response.moisture + "%";

                    // Update Status Badges
                    updateBadge("heatingBadge", response.heatingState, "გათბობა");
                    updateBadge("wateringBadge", response.wateringState, "მორწყვა");

                    var tempLimit = parseFloat(document.getElementById("tempLimit").value);
                    var tempLowerLimit = parseFloat(document.getElementById("tempLowerLimit").value);
                    var moistureLimit = parseFloat(document.getElementById("moistureLimit").value);
                    var moistureLowerLimit = parseFloat(document.getElementById("moistureLowerLimit").value);

                    var heatingToggle = document.getElementById("heatingToggle");
                    var wateringToggle = document.getElementById("wateringToggle");

                    // Temperature Logic
                    if (response.temperature > tempLimit && heatingToggle.checked) {
                        heatingToggle.checked = false;
                        toggleHeating(false);
                    } else if (response.temperature < tempLowerLimit && !heatingToggle.checked) {
                        heatingToggle.checked = true;
                        toggleHeating(false);
                    }

                    // Soil Moisture Logic
                    if (response.moisture > moistureLimit && wateringToggle.checked) {
                        wateringToggle.checked = false;
                        toggleWatering(false);
                    } else if (response.moisture < moistureLowerLimit && !wateringToggle.checked) {
                        wateringToggle.checked = true;
                        toggleWatering(false);
                    }
                }
            };
            xhr.open("GET", "/readings", true);
            xhr.send();
        }

        function updateBadge(id, state, label) {
            var badge = document.getElementById(id);
            if(state) {
                badge.innerText = label + ": ON";
                badge.className = "status-badge on";
            } else {
                badge.innerText = label + ": OFF";
                badge.className = "status-badge off";
            }
        }

        function updateLimitDisplay(sensor) {
            var lowerLimit = document.getElementById(sensor + 'LowerLimit').value;
            var upperLimit = document.getElementById(sensor + 'Limit').value;
            var unit = (sensor === 'temp') ? '°C' : '%';
            document.getElementById(sensor + 'LowerLimitDisplay').innerText = lowerLimit + unit;
            document.getElementById(sensor + 'LimitDisplay').innerText = upperLimit + unit;
        }

        function toggleDarkMode() {
            document.body.classList.toggle('dark-mode');
            var isDark = document.body.classList.contains('dark-mode');
            localStorage.setItem('theme', isDark ? 'dark' : 'light');
            
            document.getElementById('themeIcon').className = isDark ? 'fa-solid fa-sun' : 'fa-solid fa-moon';
            document.getElementById('themeText').innerText = isDark ? 'ნათელი' : 'მუქი';
        }

        function loadTheme() {
            var savedTheme = localStorage.getItem('theme');
            if (savedTheme === 'dark') {
                document.body.classList.add('dark-mode');
                document.getElementById('themeIcon').className = 'fa-solid fa-sun';
                document.getElementById('themeText').innerText = 'ნათელი';
            }
        }

        function toggleHeating(manual = true) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleHeating", true);
            xhr.send();
            if (manual) {
                var heatingToggle = document.getElementById("heatingToggle");
                localStorage.setItem("heatingEnabled", heatingToggle.checked);
            }
        }

        function toggleWatering(manual = true) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleWatering", true);
            xhr.send();
            if (manual) {
                var wateringToggle = document.getElementById("wateringToggle");
                localStorage.setItem("wateringEnabled", wateringToggle.checked);
            }
        }

        function applyToggleState() {
            var heatingEnabled = localStorage.getItem("heatingEnabled");
            var wateringEnabled = localStorage.getItem("wateringEnabled");
            if (heatingEnabled !== null) {
                document.getElementById("heatingToggle").checked = (heatingEnabled === "true");
            }
            if (wateringEnabled !== null) {
                document.getElementById("wateringToggle").checked = (wateringEnabled === "true");
            }
        }

        loadTheme();
        applyToggleState();
        setInterval(updateReadings, 2000);
    </script>
</body>
</html>
)=====";

unsigned long previousMillis = 0;
const long interval = 2000;

void setup() {
    Serial.begin(115200);
    pinMode(DHTPIN, INPUT);
    pinMode(relayPin1, OUTPUT);
    pinMode(relayPin2, OUTPUT);
    pinMode(soilMoisturePin, INPUT);

    EEPROM.begin(EEPROM_SIZE);
    
    // EEPROM-იდან მდგომარეობის წაკითხვა და შემოწმება
    uint8_t r1 = EEPROM.read(RELAY1_ADDR);
    uint8_t r2 = EEPROM.read(RELAY2_ADDR);
    
    relayState1 = (r1 == 1);
    relayState2 = (r2 == 1);

    digitalWrite(relayPin1, relayState1 ? HIGH : LOW);
    digitalWrite(relayPin2, relayState2 ? HIGH : LOW);

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("WiFi სტატიკური IP კონფიგურაცია ვერ მოხერხდა");
    }
    
    WiFi.begin(ssid, password);
    Serial.print("უერთდება WiFi-ს");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nდაკავშირებულია! IP: " + WiFi.localIP().toString());

    dht.begin();

    // მთავარი გვერდი
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", htmlPage);
    });

    // სენსორების მონაცემების JSON ენდპოინტი
    server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();
        soilMoistureValue = analogRead(soilMoisturePin);
        
        int calculatedPercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
        soilmoisturepercent = constrain(calculatedPercent, 0, 100);

        if (isnan(temperature) || isnan(humidity)) {
            request->send(500, "text/plain", "DHT სენსორიდან წაკითხვა ვერ მოხერხდა");
            return;
        }

        String response = "{\"temperature\":" + String(temperature, 1) + 
                          ",\"humidity\":" + String(humidity, 1) + 
                          ",\"moisture\":" + String(soilmoisturepercent) + 
                          ",\"heatingState\":" + String(relayState1 ? "true" : "false") + 
                          ",\"wateringState\":" + String(relayState2 ? "true" : "false") + "}";
        request->send(200, "application/json", response);
    });

    // გათბობის რელეს გადართვა
    server.on("/toggleHeating", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayState1 = !relayState1;
        digitalWrite(relayPin1, relayState1 ? HIGH : LOW);
        saveRelayState(RELAY1_ADDR, relayState1);

        String response = "{\"status\":\"" + String(relayState1 ? "ON" : "OFF") + "\"}";
        request->send(200, "application/json", response);
    });

    // მორწყვის რელეს გადართვა
    server.on("/toggleWatering", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayState2 = !relayState2;
        digitalWrite(relayPin2, relayState2 ? HIGH : LOW);
        saveRelayState(RELAY2_ADDR, relayState2);

        String response = "{\"status\":\"" + String(relayState2 ? "ON" : "OFF") + "\"}";
        request->send(200, "application/json", response);
    });

    server.begin();
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        float newT = dht.readTemperature();
        float newH = dht.readHumidity();
        soilMoistureValue = analogRead(soilMoisturePin);
        soilmoisturepercent = constrain(map(soilMoistureValue, AirValue, WaterValue, 0, 100), 0, 100);

        if (!isnan(newT)) t = newT;
        if (!isnan(newH)) h = newH;

        Serial.printf("ტემპერატურა: %.1f°C | ჰაერის ტენიანობა: %.1f%% | ნიადაგის ტენიანობა: %d (%d%%)\n", 
                      t, h, soilMoistureValue, soilmoisturepercent);
    }
}

// EEPROM-ში რელეს მდგომარეობის შენახვა
void saveRelayState(int addr, bool state) {
    EEPROM.write(addr, state ? 1 : 0);
    EEPROM.commit();
}