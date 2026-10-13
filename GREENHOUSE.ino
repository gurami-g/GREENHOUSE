#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const char* ssid = "MAGTI-1";
const char* password = "gorgadze12";

IPAddress local_IP(192, 168, 1, 101);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

#define DHTPIN D1
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define ONE_WIRE_BUS D4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature soilTempSensor(&oneWire);

#define MQ135_PIN D5

const int relayPin1 = D6; // გათბობა
const int relayPin2 = D7; // მორწყვა

Adafruit_ADS1115 ads;
BH1750 lightMeter;

bool relayState1 = false; 
bool relayState2 = false; 

float airTemp = 0.0, airHum = 0.0;
float soilTemp = 0.0;
float lux = 0.0;
int soilMoisturePct = 0;
int waterLevelPct = 0;
bool isRaining = false;
bool gasDetected = false;

const int SoilAirValue = 18000;
const int SoilWaterValue = 9000;
const int WaterEmptyValue = 5000;
const int WaterFullValue = 18000;

#define EEPROM_SIZE 2
#define RELAY1_ADDR 0
#define RELAY2_ADDR 1

void saveRelayState(int addr, bool state);

AsyncWebServer server(80);

const char* htmlPage = R"=====(
<!DOCTYPE html>
<html lang="ka">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>სათბური - სრული მართვა</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root {
            --bg: #0f172a;
            --card: #1e293b;
            --text: #f8fafc;
            --sub: #94a3b8;
            --border: #334155;
            --green: #10b981;
            --blue: #06b6d4;
            --red: #f43f5e;
            --amber: #f59e0b;
            --purple: #a855f7;
            --radius: 16px;
            --shadow: 0 10px 25px rgba(0,0,0,0.3);
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', system-ui, sans-serif; }
        body { background: var(--bg); color: var(--text); padding: 20px; min-height: 100vh; }
        .container { max-width: 1200px; margin: 0 auto; }

        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; }
        .brand { font-size: 1.6rem; font-weight: 800; color: var(--green); display: flex; align-items: center; gap: 10px; }

        .status-badge { background: rgba(16, 185, 129, 0.2); color: var(--green); padding: 6px 14px; border-radius: 20px; font-weight: 700; font-size: 0.85rem; }

        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }

        .card { background: var(--card); border: 1px solid var(--border); border-radius: var(--radius); padding: 20px; box-shadow: var(--shadow); display: flex; flex-direction: column; gap: 15px; }
        .card-title { font-weight: 700; font-size: 1.1rem; display: flex; justify-content: space-between; align-items: center; }

        .val-large { font-size: 2rem; font-weight: 800; margin: 5px 0; }
        .sub-text { font-size: 0.85rem; color: var(--sub); }

        .progress-bg { height: 8px; width: 100%; background: var(--border); border-radius: 4px; overflow: hidden; }
        .progress-fill { height: 100%; width: 0%; border-radius: 4px; transition: width 0.5s ease; }

        .chart-box { height: 120px; width: 100%; }

        .controls { display: flex; flex-direction: column; gap: 8px; }
        .control-row { display: flex; flex-direction: column; gap: 4px; }
        .control-label { display: flex; justify-content: space-between; font-size: 0.8rem; color: var(--sub); font-weight: 600; }

        input[type=range] { width: 100%; height: 6px; background: var(--border); border-radius: 3px; outline: none; accent-color: var(--green); cursor: pointer; }

        .relay-box { display: flex; justify-content: space-between; align-items: center; padding-top: 10px; border-top: 1px solid var(--border); }
        .badge { padding: 4px 10px; border-radius: 20px; font-size: 0.75rem; font-weight: 700; }
        .badge.on { background: rgba(16, 185, 129, 0.2); color: var(--green); }
        .badge.off { background: rgba(244, 63, 94, 0.2); color: var(--red); }

        .switch { position: relative; width: 44px; height: 24px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #475569; transition: .3s; border-radius: 34px; }
        .slider:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
        input:checked + .slider { background-color: var(--green); }
        input:checked + .slider:before { transform: translateX(20px); }
    </style>
</head>
<body>

    <div class="container">
        <div class="header">
            <div class="brand"><i class="fa-solid fa-leaf"></i> სათბური - 6 სენსორი</div>
            <div class="status-badge">ONLINE</div>
        </div>

        <div class="grid">
            
            <!-- 1. ჰაერის კლიმატი -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-temperature-half" style="color: var(--red);"></i> ჰაერის კლიმატი</span>
                    <span id="heatBadge" class="badge off">გათბობა: OFF</span>
                </div>
                <div style="display: flex; justify-content: space-between;">
                    <div>
                        <div class="sub-text">ტემპერატურა</div>
                        <div id="airTemp" class="val-large" style="color: var(--red);">--°C</div>
                    </div>
                    <div>
                        <div class="sub-text">ტენიანობა</div>
                        <div id="airHum" class="val-large" style="color: var(--blue);">--%</div>
                    </div>
                </div>
                <div class="chart-box"><canvas id="climateChart"></canvas></div>
                <div class="controls">
                    <div class="control-row">
                        <div class="control-label"><span>ქვედა ზღვარი</span><span id="tempLowerLimitDisplay">18°C</span></div>
                        <input type="range" id="tempLowerLimit" min="0" max="50" value="18" oninput="updateLimits('temp')">
                    </div>
                    <div class="control-row">
                        <div class="control-label"><span>ზედა ზღვარი</span><span id="tempLimitDisplay">28°C</span></div>
                        <input type="range" id="tempLimit" min="0" max="50" value="28" oninput="updateLimits('temp')">
                    </div>
                </div>
                <div class="relay-box">
                    <span style="font-size: 0.85rem; font-weight: 600;">გათბობის რელე</span>
                    <label class="switch">
                        <input type="checkbox" id="heatingToggle" onclick="toggleHeating()">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>

            <!-- 2. ნიადაგი & ფესვები -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-droplet" style="color: var(--amber);"></i> ნიადაგი & ფესვები</span>
                    <span id="waterBadge" class="badge off">მორწყვა: OFF</span>
                </div>
                <div style="display: flex; justify-content: space-between;">
                    <div>
                        <div class="sub-text">ნიადაგის ტენიანობა</div>
                        <div id="soilMoist" class="val-large" style="color: var(--amber);">--%</div>
                    </div>
                    <div>
                        <div class="sub-text">ფესვების ტემპ.</div>
                        <div id="soilTemp" class="val-large" style="color: var(--purple);">--°C</div>
                    </div>
                </div>
                <div class="chart-box"><canvas id="soilChart"></canvas></div>
                <div class="controls">
                    <div class="control-row">
                        <div class="control-label"><span>ქვედა ზღვარი</span><span id="moistureLowerLimitDisplay">30%</span></div>
                        <input type="range" id="moistureLowerLimit" min="0" max="100" value="30" oninput="updateLimits('moisture')">
                    </div>
                    <div class="control-row">
                        <div class="control-label"><span>ზედა ზღვარი</span><span id="moistureLimitDisplay">70%</span></div>
                        <input type="range" id="moistureLimit" min="0" max="100" value="70" oninput="updateLimits('moisture')">
                    </div>
                </div>
                <div class="relay-box">
                    <span style="font-size: 0.85rem; font-weight: 600;">მორწყვის რელე</span>
                    <label class="switch">
                        <input type="checkbox" id="wateringToggle" onclick="toggleWatering()">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>

            <!-- 3. განათების სენსორი -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-sun" style="color: #eab308;"></i> განათება (BH1750)</span>
                </div>
                <div class="sub-text">სინათლის ინტენსივობა</div>
                <div id="lux" class="val-large" style="color: #eab308;">-- Lux</div>
                <div class="progress-bg"><div id="luxBar" class="progress-fill" style="background: #eab308;"></div></div>
            </div>

            <!-- 4. წყლის ავზი -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-faucet-drip" style="color: var(--blue);"></i> წყლის ავზი</span>
                </div>
                <div class="sub-text">ავზის შევსების დონე</div>
                <div id="waterLevel" class="val-large" style="color: var(--blue);">--%</div>
                <div class="progress-bg"><div id="waterBar" class="progress-fill" style="background: var(--blue);"></div></div>
            </div>

            <!-- 5. წვიმის სენსორი -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-cloud-rain" style="color: var(--blue);"></i> ნალექი</span>
                </div>
                <div class="sub-text">წვიმის სტატუსი</div>
                <div id="rainStatus" class="val-large" style="color: var(--sub);">--</div>
            </div>

            <!-- 6. უსაფრთხოება -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-shield-halved" style="color: var(--red);"></i> გაზი / კვამლი</span>
                </div>
                <div class="sub-text">ჰაერის უსაფრთხოება (MQ-135)</div>
                <div id="gasStatus" class="val-large" style="color: var(--green);">ნორმა</div>
            </div>

        </div>
    </div>

    <script>
        const maxPoints = 10;
        const labels = [];

        const cCtx = document.getElementById('climateChart').getContext('2d');
        const climateChart = new Chart(cCtx, {
            type: 'line',
            data: { labels: labels, datasets: [
                { label: 'T', data: [], borderColor: '#f43f5e', fill: false, tension: 0.3 },
                { label: 'H', data: [], borderColor: '#06b6d4', fill: false, tension: 0.3 }
            ]},
            options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { display: false } } }
        });

        const sCtx = document.getElementById('soilChart').getContext('2d');
        const soilChart = new Chart(sCtx, {
            type: 'line',
            data: { labels: labels, datasets: [
                { label: 'SM', data: [], borderColor: '#f59e0b', fill: false, tension: 0.3 },
                { label: 'ST', data: [], borderColor: '#a855f7', fill: false, tension: 0.3 }
            ]},
            options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { display: false } } }
        });

        function updateReadings() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (xhr.readyState == XMLHttpRequest.DONE && xhr.status == 200) {
                    var data = JSON.parse(xhr.responseText);

                    document.getElementById("airTemp").innerText = data.airTemp.toFixed(1) + '°C';
                    document.getElementById("airHum").innerText = data.airHum.toFixed(1) + "%";
                    document.getElementById("soilMoist").innerText = data.soilMoist + "%";
                    document.getElementById("soilTemp").innerText = data.soilTemp.toFixed(1) + '°C';
                    document.getElementById("lux").innerText = Math.round(data.lux) + " Lux";
                    document.getElementById("waterLevel").innerText = data.waterLevel + "%";

                    document.getElementById("luxBar").style.width = Math.min((data.lux / 50000) * 100, 100) + "%";
                    document.getElementById("waterBar").style.width = Math.min(Math.max(data.waterLevel, 0), 100) + "%";

                    document.getElementById("rainStatus").innerText = data.isRaining ? "წვიმს 🌧️" : "მშრალია ☀️";
                    document.getElementById("gasStatus").innerText = data.gasDetected ? "საფრთხე! 🚨" : "ნორმა ✅";
                    document.getElementById("gasStatus").style.color = data.gasDetected ? "var(--red)" : "var(--green)";

                    updateBadge("heatBadge", data.heatingState, "გათბობა");
                    updateBadge("waterBadge", data.wateringState, "მორწყვა");

                    const timeStr = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
                    if (labels.length >= maxPoints) labels.shift();
                    labels.push(timeStr);

                    if (climateChart.data.datasets[0].data.length >= maxPoints) {
                        climateChart.data.datasets[0].data.shift();
                        climateChart.data.datasets[1].data.shift();
                        soilChart.data.datasets[0].data.shift();
                        soilChart.data.datasets[1].data.shift();
                    }

                    climateChart.data.datasets[0].data.push(data.airTemp);
                    climateChart.data.datasets[1].data.push(data.airHum);
                    soilChart.data.datasets[0].data.push(data.soilMoist);
                    soilChart.data.datasets[1].data.push(data.soilTemp);

                    climateChart.update();
                    soilChart.update();
                }
            };
            // Cache-Busting მოთხოვნა
            xhr.open("GET", "/readings?t=" + new Date().getTime(), true);
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

        function toggleHeating() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleHeating", true);
            xhr.send();
        }

        function toggleWatering() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleWatering", true);
            xhr.send();
        }

        setInterval(updateReadings, 2000);
    </script>
</body>
</html>
)=====";

void setup() {
    Serial.begin(115200);

    pinMode(DHTPIN, INPUT);
    pinMode(MQ135_PIN, INPUT);
    pinMode(relayPin1, OUTPUT);
    pinMode(relayPin2, OUTPUT);

    Wire.begin(D2, D3);
    
    if (!ads.begin()) {
        Serial.println("ADS1115 ვერ მოიძებნა!");
    }
    
    lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
    soilTempSensor.begin();

    EEPROM.begin(EEPROM_SIZE);
    uint8_t r1 = EEPROM.read(RELAY1_ADDR);
    uint8_t r2 = EEPROM.read(RELAY2_ADDR);
    
    relayState1 = (r1 == 1);
    relayState2 = (r2 == 1);

    digitalWrite(relayPin1, relayState1 ? HIGH : LOW);
    digitalWrite(relayPin2, relayState2 ? HIGH : LOW);

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("WiFi IP კონფიგურაცია ვერ მოხერხდა");
    }
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    dht.begin();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", htmlPage);
    });

    server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
        float tRead = dht.readTemperature();
        float hRead = dht.readHumidity();
        if (!isnan(tRead)) airTemp = tRead;
        if (!isnan(hRead)) airHum = hRead;

        soilTempSensor.requestTemperatures();
        float stRead = soilTempSensor.getTempCByIndex(0);
        if (stRead > -55 && stRead < 125) soilTemp = stRead;

        lux = lightMeter.readLightLevel();
        gasDetected = (digitalRead(MQ135_PIN) == LOW);

        int16_t adc0 = ads.readADC_SingleEnded(0);
        int16_t adc1 = ads.readADC_SingleEnded(1);
        int16_t adc2 = ads.readADC_SingleEnded(2);

        soilMoisturePct = constrain(map(adc0, SoilAirValue, SoilWaterValue, 0, 100), 0, 100);
        waterLevelPct = constrain(map(adc1, WaterEmptyValue, WaterFullValue, 0, 100), 0, 100);
        isRaining = (adc2 < 12000);

        String response = "{";
        response += "\"airTemp\":" + String(airTemp, 1) + ",";
        response += "\"airHum\":" + String(airHum, 1) + ",";
        response += "\"soilTemp\":" + String(soilTemp, 1) + ",";
        response += "\"soilMoist\":" + String(soilMoisturePct) + ",";
        response += "\"lux\":" + String(lux, 0) + ",";
        response += "\"waterLevel\":" + String(waterLevelPct) + ",";
        response += "\"isRaining\":" + String(isRaining ? "true" : "false") + ",";
        response += "\"gasDetected\":" + String(gasDetected ? "true" : "false") + ",";
        response += "\"heatingState\":" + String(relayState1 ? "true" : "false") + ",";
        response += "\"wateringState\":" + String(relayState2 ? "true" : "false");
        response += "}";

        request->send(200, "application/json", response);
    });

    server.on("/toggleHeating", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayState1 = !relayState1;
        digitalWrite(relayPin1, relayState1 ? HIGH : LOW);
        saveRelayState(RELAY1_ADDR, relayState1);
        request->send(200, "application/json", "{\"status\":\"OK\"}");
    });

    server.on("/toggleWatering", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayState2 = !relayState2;
        digitalWrite(relayPin2, relayState2 ? HIGH : LOW);
        saveRelayState(RELAY2_ADDR, relayState2);
        request->send(200, "application/json", "{\"status\":\"OK\"}");
    });

    server.begin();
}

void loop() {
}

void saveRelayState(int addr, bool state) {
    EEPROM.write(addr, state ? 1 : 0);
    EEPROM.commit();
}