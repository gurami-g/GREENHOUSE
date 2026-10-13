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
    <title>სათბურის მართვა Pro</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root {
            --bg-color: #f0f4f8;
            --card-bg: #ffffff;
            --text-main: #1e293b;
            --text-sub: #64748b;
            --green: #10b981;
            --blue: #3b82f6;
            --red: #ef4444;
            --orange: #f59e0b;
            --border: #e2e8f0;
            --radius: 18px;
            --shadow: 0 10px 30px -5px rgba(0,0,0,0.05);
        }

        body.dark-mode {
            --bg-color: #0f172a;
            --card-bg: #1e293b;
            --text-main: #f8fafc;
            --text-sub: #94a3b8;
            --border: #334155;
            --shadow: 0 10px 30px -5px rgba(0,0,0,0.3);
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', system-ui, sans-serif; transition: background 0.3s, color 0.3s; }
        body { background: var(--bg-color); color: var(--text-main); padding: 20px; min-height: 100vh; }

        .header { max-width: 1100px; margin: 0 auto 25px auto; display: flex; justify-content: space-between; align-items: center; }
        .header h1 { font-size: 1.8rem; font-weight: 700; color: var(--green); display: flex; align-items: center; gap: 12px; }

        .theme-btn { background: var(--card-bg); border: 1px solid var(--border); color: var(--text-main); padding: 10px 18px; border-radius: 30px; cursor: pointer; font-weight: 600; display: flex; align-items: center; gap: 8px; box-shadow: var(--shadow); }

        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(340px, 1fr)); gap: 24px; max-width: 1100px; margin: 0 auto; }

        .card { background: var(--card-bg); border-radius: var(--radius); border: 1px solid var(--border); padding: 24px; box-shadow: var(--shadow); display: flex; flex-direction: column; gap: 18px; }
        .card-title { display: flex; justify-content: space-between; align-items: center; font-weight: 700; font-size: 1.1rem; }
        .card-title i { margin-right: 8px; }

        .metrics { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
        .metric-card { background: var(--bg-color); border-radius: 12px; padding: 14px; text-align: center; }
        .metric-title { font-size: 0.85rem; color: var(--text-sub); margin-bottom: 4px; }
        .metric-val { font-size: 1.6rem; font-weight: 800; }

        .chart-container { height: 160px; width: 100%; position: relative; }

        .control-item { display: flex; flex-direction: column; gap: 6px; font-size: 0.9rem; color: var(--text-sub); }
        .slider-row { display: flex; align-items: center; gap: 12px; }
        .slider-row input[type=range] { flex: 1; accent-color: var(--green); cursor: pointer; }
        .val-badge { font-weight: 700; color: var(--text-main); min-width: 45px; text-align: right; }

        .switch-row { display: flex; justify-content: space-between; align-items: center; padding-top: 12px; border-top: 1px solid var(--border); }
        .badge { padding: 5px 12px; border-radius: 20px; font-size: 0.8rem; font-weight: 700; }
        .badge.on { background: rgba(16, 185, 129, 0.15); color: var(--green); }
        .badge.off { background: rgba(239, 68, 68, 0.15); color: var(--red); }

        .toggle-sw { position: relative; width: 50px; height: 26px; }
        .toggle-sw input { opacity: 0; width: 0; height: 0; }
        .sw-slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #cbd5e1; transition: .3s; border-radius: 30px; }
        .sw-slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 3px; bottom: 3px; background: white; transition: .3s; border-radius: 50%; }
        input:checked + .sw-slider { background: var(--green); }
        input:checked + .sw-slider:before { transform: translateX(24px); }
    </style>
</head>
<body>

    <div class="header">
        <h1><i class="fa-solid fa-seedling"></i> სათბურის მართვა</h1>
        <button class="theme-btn" onclick="toggleTheme()">
            <i class="fa-solid fa-moon" id="tIcon"></i>
            <span id="tText">მუქი</span>
        </button>
    </div>

    <div class="grid">
        <!-- Climate Control -->
        <div class="card">
            <div class="card-title">
                <span><i class="fa-solid fa-temperature-arrow-up" style="color: var(--red);"></i> ჰაერის კლიმატი</span>
                <span id="heatBadge" class="badge off">გათბობა: OFF</span>
            </div>

            <div class="metrics">
                <div class="metric-card">
                    <div class="metric-title">ტემპერატურა</div>
                    <div id="temp" class="metric-val" style="color: var(--red);">--°C</div>
                </div>
                <div class="metric-card">
                    <div class="metric-title">ჰაერის ტენიანობა</div>
                    <div id="humid" class="metric-val" style="color: var(--blue);">--%</div>
                </div>
            </div>

            <div class="chart-container">
                <canvas id="climateChart"></canvas>
            </div>

            <div class="control-item">
                <label>ქვედა ზღვარი (ჩართვა):</label>
                <div class="slider-row">
                    <input type="range" id="tempLowerLimit" min="0" max="50" value="18" oninput="updateLimits('temp')">
                    <span id="tempLowerLimitDisplay" class="val-badge">18°C</span>
                </div>
            </div>

            <div class="control-item">
                <label>ზედა ზღვარი (გამორთვა):</label>
                <div class="slider-row">
                    <input type="range" id="tempLimit" min="0" max="50" value="28" oninput="updateLimits('temp')">
                    <span id="tempLimitDisplay" class="val-badge">28°C</span>
                </div>
            </div>

            <div class="switch-row">
                <span>გათბობის მართვა</span>
                <label class="toggle-sw">
                    <input type="checkbox" id="heatingToggle" onclick="toggleHeating()">
                    <span class="sw-slider"></span>
                </label>
            </div>
        </div>

        <!-- Soil Moisture -->
        <div class="card">
            <div class="card-title">
                <span><i class="fa-solid fa-faucet-drip" style="color: var(--orange);"></i> ნიადაგის მორწყვა</span>
                <span id="waterBadge" class="badge off">მორწყვა: OFF</span>
            </div>

            <div class="metrics" style="grid-template-columns: 1fr;">
                <div class="metric-card">
                    <div class="metric-title">ნიადაგის ტენიანობა</div>
                    <div id="moisture" class="metric-val" style="color: var(--orange);">--%</div>
                </div>
            </div>

            <div class="chart-container">
                <canvas id="soilChart"></canvas>
            </div>

            <div class="control-item">
                <label>ქვედა ზღვარი (ჩართვა):</label>
                <div class="slider-row">
                    <input type="range" id="moistureLowerLimit" min="0" max="100" value="30" oninput="updateLimits('moisture')">
                    <span id="moistureLowerLimitDisplay" class="val-badge">30%</span>
                </div>
            </div>

            <div class="control-item">
                <label>ზედა ზღვარი (გამორთვა):</label>
                <div class="slider-row">
                    <input type="range" id="moistureLimit" min="0" max="100" value="70" oninput="updateLimits('moisture')">
                    <span id="moistureLimitDisplay" class="val-badge">70%</span>
                </div>
            </div>

            <div class="switch-row">
                <span>მორწყვის მართვა</span>
                <label class="toggle-sw">
                    <input type="checkbox" id="wateringToggle" onclick="toggleWatering()">
                    <span class="sw-slider"></span>
                </label>
            </div>
        </div>
    </div>

    <script>
        // Charts Configuration
        const maxDataPoints = 15;
        const timeLabels = [];

        const climateCtx = document.getElementById('climateChart').getContext('2d');
        const climateChart = new Chart(climateCtx, {
            type: 'line',
            data: {
                labels: timeLabels,
                datasets: [
                    { label: 'Temp (°C)', data: [], borderColor: '#ef4444', tension: 0.4, borderWidth: 2, pointRadius: 2 },
                    { label: 'Humid (%)', data: [], borderColor: '#3b82f6', tension: 0.4, borderWidth: 2, pointRadius: 2 }
                ]
            },
            options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { grid: { display: false } } } }
        });

        const soilCtx = document.getElementById('soilChart').getContext('2d');
        const soilChart = new Chart(soilCtx, {
            type: 'line',
            data: {
                labels: timeLabels,
                datasets: [
                    { label: 'Moisture (%)', data: [], borderColor: '#f59e0b', tension: 0.4, borderWidth: 2, pointRadius: 2 }
                ]
            },
            options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { min: 0, max: 100, grid: { display: false } } } }
        });

        function updateReadings() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (xhr.readyState == XMLHttpRequest.DONE && xhr.status == 200) {
                    var data = JSON.parse(xhr.responseText);

                    document.getElementById("temp").innerText = data.temperature.toFixed(1) + '°C';
                    document.getElementById("humid").innerText = data.humidity.toFixed(1) + "%";
                    document.getElementById("moisture").innerText = data.moisture + "%";

                    updateBadge("heatBadge", data.heatingState, "გათბობა");
                    updateBadge("waterBadge", data.wateringState, "მორწყვა");

                    // Push Chart Data
                    const now = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
                    if (timeLabels.length >= maxDataPoints) timeLabels.shift();
                    timeLabels.push(now);

                    if (climateChart.data.datasets[0].data.length >= maxDataPoints) {
                        climateChart.data.datasets[0].data.shift();
                        climateChart.data.datasets[1].data.shift();
                        soilChart.data.datasets[0].data.shift();
                    }

                    climateChart.data.datasets[0].data.push(data.temperature);
                    climateChart.data.datasets[1].data.push(data.humidity);
                    soilChart.data.datasets[0].data.push(data.moisture);

                    climateChart.update();
                    soilChart.update();

                    // Automation Thresholds
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
            document.getElementById('tText').innerText = isDark ? 'ნათელი' : 'მუქი';
        }

        function loadTheme() {
            if (localStorage.getItem('theme') === 'dark') {
                document.body.classList.add('dark-mode');
                document.getElementById('tIcon').className = 'fa-solid fa-sun';
                document.getElementById('tText').innerText = 'ნათელი';
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