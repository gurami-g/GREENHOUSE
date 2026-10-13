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
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root {
            --bg: #f8fafc;
            --card: #ffffff;
            --text: #0f172a;
            --sub: #64748b;
            --border: #e2e8f0;
            --green: #10b981;
            --blue: #06b6d4;
            --red: #f43f5e;
            --amber: #f59e0b;
            --radius: 20px;
            --shadow: 0 20px 25px -5px rgba(0,0,0,0.05), 0 8px 10px -6px rgba(0,0,0,0.01);
        }

        body.dark-mode {
            --bg: #0b0f19;
            --card: #151c2c;
            --text: #f1f5f9;
            --sub: #94a3b8;
            --border: #1e293b;
            --shadow: 0 20px 25px -5px rgba(0,0,0,0.4);
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Inter', system-ui, -apple-system, sans-serif; transition: background 0.3s, color 0.3s; }
        body { background: var(--bg); color: var(--text); padding: 20px; min-height: 100vh; }

        .container { max-width: 1000px; margin: 0 auto; }
        
        /* Top Navigation Bar */
        .navbar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; padding: 10px 0; }
        .brand { display: flex; align-items: center; gap: 12px; font-size: 1.5rem; font-weight: 800; color: var(--green); }
        .brand i { font-size: 1.8rem; }
        
        .nav-actions { display: flex; align-items: center; gap: 15px; }
        .status-dot { display: flex; align-items: center; gap: 8px; font-size: 0.85rem; font-weight: 600; padding: 6px 14px; border-radius: 20px; background: rgba(16, 185, 129, 0.1); color: var(--green); }
        .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--green); animation: pulse 2s infinite; }
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.3; } 100% { opacity: 1; } }

        .theme-toggle { background: var(--card); border: 1px solid var(--border); color: var(--text); padding: 8px 16px; border-radius: 30px; cursor: pointer; font-weight: 600; display: flex; align-items: center; gap: 8px; box-shadow: var(--shadow); }

        /* Main Dashboard Grid */
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 24px; }

        .card { background: var(--card); border: 1px solid var(--border); border-radius: var(--radius); padding: 24px; box-shadow: var(--shadow); display: flex; flex-direction: column; gap: 20px; }
        .card-header { display: flex; justify-content: space-between; align-items: center; }
        .card-title { font-size: 1.1rem; font-weight: 700; display: flex; align-items: center; gap: 10px; }

        /* Metric Displays & Progress Bars */
        .metrics-container { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
        .metric-box { background: rgba(0,0,0,0.02); border-radius: 14px; padding: 16px; position: relative; overflow: hidden; }
        .dark-mode .metric-box { background: rgba(255,255,255,0.02); }
        .metric-label { font-size: 0.8rem; color: var(--sub); font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; }
        .metric-value { font-size: 1.8rem; font-weight: 800; margin: 6px 0; }
        
        .progress-bg { height: 6px; width: 100%; background: var(--border); border-radius: 3px; overflow: hidden; }
        .progress-fill { height: 100%; width: 0%; border-radius: 3px; transition: width 0.5s ease; }

        .chart-box { height: 150px; width: 100%; }

        /* Controls & Sliders */
        .controls { display: flex; flex-direction: column; gap: 12px; }
        .control-row { display: flex; flex-direction: column; gap: 6px; }
        .control-label { display: flex; justify-content: space-between; font-size: 0.85rem; color: var(--sub); font-weight: 600; }
        .control-label span:last-child { color: var(--text); font-weight: 700; }

        input[type=range] { width: 100%; height: 6px; background: var(--border); border-radius: 3px; outline: none; accent-color: var(--green); cursor: pointer; }

        /* Relay Switches */
        .relay-box { display: flex; justify-content: space-between; align-items: center; padding-top: 15px; border-top: 1px solid var(--border); }
        .badge { padding: 5px 12px; border-radius: 30px; font-size: 0.75rem; font-weight: 700; }
        .badge.on { background: rgba(16, 185, 129, 0.15); color: var(--green); }
        .badge.off { background: rgba(244, 63, 94, 0.15); color: var(--red); }

        .switch { position: relative; width: 48px; height: 26px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #cbd5e1; transition: .3s; border-radius: 34px; }
        .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
        input:checked + .slider { background-color: var(--green); }
        input:checked + .slider:before { transform: translateX(22px); }
    </style>
</head>
<body>

    <div class="container">
        <!-- Top Nav -->
        <div class="navbar">
            <div class="brand">
                <i class="fa-solid fa-plant-wilt"></i>
                <span>სათბური Pro</span>
            </div>
            <div class="nav-actions">
                <div class="status-dot">
                    <div class="dot"></div>
                    <span id="connStatus">ONLINE</span>
                </div>
                <button class="theme-toggle" onclick="toggleTheme()">
                    <i class="fa-solid fa-moon" id="tIcon"></i>
                </button>
            </div>
        </div>

        <!-- Dashboard Grid -->
        <div class="grid">
            
            <!-- Climate Control Card -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <i class="fa-solid fa-temperature-half" style="color: var(--red);"></i>
                        ჰაერის კლიმატი
                    </div>
                    <span id="heatBadge" class="badge off">გათბობა: OFF</span>
                </div>

                <div class="metrics-container">
                    <div class="metric-box">
                        <div class="metric-label">ტემპერატურა</div>
                        <div id="temp" class="metric-value" style="color: var(--red);">--°C</div>
                        <div class="progress-bg"><div id="tempBar" class="progress-fill" style="background: var(--red);"></div></div>
                    </div>
                    <div class="metric-box">
                        <div class="metric-label">ტენიანობა</div>
                        <div id="humid" class="metric-value" style="color: var(--blue);">--%</div>
                        <div class="progress-bg"><div id="humidBar" class="progress-fill" style="background: var(--blue);"></div></div>
                    </div>
                </div>

                <div class="chart-box">
                    <canvas id="climateChart"></canvas>
                </div>

                <div class="controls">
                    <div class="control-row">
                        <div class="control-label">
                            <span>ქვედა ზღვარი (ჩართვა)</span>
                            <span id="tempLowerLimitDisplay">18°C</span>
                        </div>
                        <input type="range" id="tempLowerLimit" min="0" max="50" value="18" oninput="updateLimits('temp')">
                    </div>
                    <div class="control-row">
                        <div class="control-label">
                            <span>ზედა ზღვარი (გამორთვა)</span>
                            <span id="tempLimitDisplay">28°C</span>
                        </div>
                        <input type="range" id="tempLimit" min="0" max="50" value="28" oninput="updateLimits('temp')">
                    </div>
                </div>

                <div class="relay-box">
                    <span style="font-size: 0.9rem; font-weight: 600;">გათბობის ავტო/ხელით მართვა</span>
                    <label class="switch">
                        <input type="checkbox" id="heatingToggle" onclick="toggleHeating()">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>

            <!-- Soil Moisture Card -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <i class="fa-solid fa-droplet" style="color: var(--amber);"></i>
                        ნიადაგის მორწყვა
                    </div>
                    <span id="waterBadge" class="badge off">მორწყვა: OFF</span>
                </div>

                <div class="metrics-container" style="grid-template-columns: 1fr;">
                    <div class="metric-box">
                        <div class="metric-label">ნიადაგის ტენიანობა</div>
                        <div id="moisture" class="metric-value" style="color: var(--amber);">--%</div>
                        <div class="progress-bg"><div id="soilBar" class="progress-fill" style="background: var(--amber);"></div></div>
                    </div>
                </div>

                <div class="chart-box">
                    <canvas id="soilChart"></canvas>
                </div>

                <div class="controls">
                    <div class="control-row">
                        <div class="control-label">
                            <span>ქვედა ზღვარი (ჩართვა)</span>
                            <span id="moistureLowerLimitDisplay">30%</span>
                        </div>
                        <input type="range" id="moistureLowerLimit" min="0" max="100" value="30" oninput="updateLimits('moisture')">
                    </div>
                    <div class="control-row">
                        <div class="control-label">
                            <span>ზედა ზღვარი (გამორთვა)</span>
                            <span id="moistureLimitDisplay">70%</span>
                        </div>
                        <input type="range" id="moistureLimit" min="0" max="100" value="70" oninput="updateLimits('moisture')">
                    </div>
                </div>

                <div class="relay-box">
                    <span style="font-size: 0.9rem; font-weight: 600;">მორწყვის ავტო/ხელით მართვა</span>
                    <label class="switch">
                        <input type="checkbox" id="wateringToggle" onclick="toggleWatering()">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>

        </div>
    </div>

    <script>
        const maxPoints = 12;
        const labels = [];

        // Climate Chart Initialization
        const cCtx = document.getElementById('climateChart').getContext('2d');
        const climateChart = new Chart(cCtx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    { label: 'Temp', data: [], borderColor: '#f43f5e', backgroundColor: 'rgba(244, 63, 94, 0.1)', fill: true, tension: 0.4, borderWidth: 2 },
                    { label: 'Humid', data: [], borderColor: '#06b6d4', backgroundColor: 'rgba(6, 182, 212, 0.1)', fill: true, tension: 0.4, borderWidth: 2 }
                ]
            },
            options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { display: false } } }
        });

        // Soil Chart Initialization
        const sCtx = document.getElementById('soilChart').getContext('2d');
        const soilChart = new Chart(sCtx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    { label: 'Soil', data: [], borderColor: '#f59e0b', backgroundColor: 'rgba(245, 158, 11, 0.1)', fill: true, tension: 0.4, borderWidth: 2 }
                ]
            },
            options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { min: 0, max: 100, display: false } } }
        });

        function updateReadings() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (xhr.readyState == XMLHttpRequest.DONE) {
                    if (xhr.status == 200) {
                        var data = JSON.parse(xhr.responseText);

                        // Text Updates
                        document.getElementById("temp").innerText = data.temperature.toFixed(1) + '°C';
                        document.getElementById("humid").innerText = data.humidity.toFixed(1) + "%";
                        document.getElementById("moisture").innerText = data.moisture + "%";

                        // Progress Bar Updates
                        document.getElementById("tempBar").style.width = Math.min(Math.max((data.temperature / 50) * 100, 0), 100) + "%";
                        document.getElementById("humidBar").style.width = Math.min(Math.max(data.humidity, 0), 100) + "%";
                        document.getElementById("soilBar").style.width = Math.min(Math.max(data.moisture, 0), 100) + "%";

                        // Status Badges
                        updateBadge("heatBadge", data.heatingState, "გათბობა");
                        updateBadge("waterBadge", data.wateringState, "მორწყვა");

                        // Push Live Data to Charts
                        const timeStr = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
                        if (labels.length >= maxPoints) labels.shift();
                        labels.push(timeStr);

                        if (climateChart.data.datasets[0].data.length >= maxPoints) {
                            climateChart.data.datasets[0].data.shift();
                            climateChart.data.datasets[1].data.shift();
                            soilChart.data.datasets[0].data.shift();
                        }

                        climateChart.data.datasets[0].data.push(data.temperature);
                        climateChart.data.datasets[1].data.push(data.humidity);
                        soilChart.data.datasets[0].data.push(data.moisture);

                        climateChart.update();
                        soilChart.update();

                        // Automation Hysteresis
                        var tUpper = parseFloat(document.getElementById("tempLimit").value);
                        var tLower = parseFloat(document.getElementById("tempLowerLimit").value);
                        var mUpper = parseFloat(document.getElementById("moistureLimit").value);
                        var mLower = parseFloat(document.getElementById("moistureLowerLimit").value);

                        var hToggle = document.getElementById("heatingToggle");
                        var wToggle = document.getElementById("wateringToggle");

                        if (data.temperature > tUpper && hToggle.checked) { hToggle.checked = false; toggleHeating(false); }
                        else if (data.temperature < tLower && !hToggle.checked) { hToggle.checked = true; toggleHeating(false); }

                        if (data.moisture > mUpper && wToggle.checked) { wToggle.checked = false; toggleWatering(false); }
                        else if (data.moisture < mLower && !wToggle.checked) { wToggle.checked = true; toggleWatering(false); }
                    }
                }
            };
            xhr.open("GET", "/readings", true);
            xhr.send();
        }

        function updateBadge(id, state, text) {
            var el = document.getElementById(id);
            el.innerText = text + (state ? ": ON" : ": OFF");
            el.className = "badge " + (state ? "on" : "off");
        }

        function updateLimits(type) {
            var low = document.getElementById(type + 'LowerLimit').value;
            var up = document.getElementById(type + 'Limit').value;
            var unit = type === 'temp' ? '°C' : '%';
            document.getElementById(type + 'LowerLimitDisplay').innerText = low + unit;
            document.getElementById(type + 'LimitDisplay').innerText = up + unit;
        }

        function toggleTheme() {
            document.body.classList.toggle('dark-mode');
            var isDark = document.body.classList.contains('dark-mode');
            localStorage.setItem('theme', isDark ? 'dark' : 'light');
            document.getElementById('tIcon').className = isDark ? 'fa-solid fa-sun' : 'fa-solid fa-moon';
        }

        function loadTheme() {
            if (localStorage.getItem('theme') === 'dark') {
                document.body.classList.add('dark-mode');
                document.getElementById('tIcon').className = 'fa-solid fa-sun';
            }
        }

        function toggleHeating(manual = true) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleHeating", true);
            xhr.send();
            if (manual) localStorage.setItem("heatingEnabled", document.getElementById("heatingToggle").checked);
        }

        function toggleWatering(manual = true) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleWatering", true);
            xhr.send();
            if (manual) localStorage.setItem("wateringEnabled", document.getElementById("wateringToggle").checked);
        }

        function applySavedToggles() {
            var h = localStorage.getItem("heatingEnabled");
            var w = localStorage.getItem("wateringEnabled");
            if (h !== null) document.getElementById("heatingToggle").checked = (h === "true");
            if (w !== null) document.getElementById("wateringToggle").checked = (w === "true");
        }

        loadTheme();
        applySavedToggles();
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

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", htmlPage);
    });

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

    server.on("/toggleHeating", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayState1 = !relayState1;
        digitalWrite(relayPin1, relayState1 ? HIGH : LOW);
        saveRelayState(RELAY1_ADDR, relayState1);

        String response = "{\"status\":\"" + String(relayState1 ? "ON" : "OFF") + "\"}";
        request->send(200, "application/json", response);
    });

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

void saveRelayState(int addr, bool state) {
    EEPROM.write(addr, state ? 1 : 0);
    EEPROM.commit();
}