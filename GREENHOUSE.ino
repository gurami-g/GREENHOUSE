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

// --- WiFi პარამეტრები ---
const char* ssid = "MAGTI-1";
const char* password = "gorgadze12";

IPAddress local_IP(192, 168, 1, 101);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// --- პინების განმარტება ---
#define DHTPIN D1
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define ONE_WIRE_BUS D4   // DS18B20 ნიადაგის ტემპერატურა
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature soilTempSensor(&oneWire);

#define MQ135_PIN D5      // გაზი/კვამლი (Digital DO)

const int relayPin1 = D6;    // Relay 1: ჰაერის გათბობა
const int relayPinSoil = D7; // Relay 2: ნიადაგის გათბობის კაბელი (ადგილები გაიცვალა)
const int relayPinWater = D8; // Relay 3: მორწყვა (ადგილები გაიცვალა)

// --- I2C მოდულები ---
Adafruit_ADS1115 ads;
BH1750 lightMeter;

// --- ცვლადები ---
bool relayState1 = false; 
bool relayStateSoil = false;
bool relayStateWater = false; 

float airTemp = 0.0, airHum = 0.0;
float soilTemp = 0.0;
float lux = 0.0;
int soilMoisturePct = 0;
int waterLevelPct = 0;
bool isRaining = false;
bool gasDetected = false;

// ADS1115 კალიბრაციის მნიშვნელობები
const int SoilAirValue = 18000;
const int SoilWaterValue = 9000;
const int WaterEmptyValue = 5000;
const int WaterFullValue = 18000;

#define EEPROM_SIZE 3
#define RELAY1_ADDR 0
#define RELAY_SOIL_ADDR 1
#define RELAY_WATER_ADDR 2

void saveRelayState(int addr, bool state);

AsyncWebServer server(80);

const char* htmlPage = R"=====(
<!DOCTYPE html>
<html lang="ka">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>სათბური Pro</title>
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

        body.dark-mode {
            --bg: #0f172a;
            --card: #1e293b;
            --text: #f8fafc;
            --sub: #94a3b8;
            --border: #334155;
            --shadow: 0 10px 25px rgba(0,0,0,0.3);
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', system-ui, sans-serif; transition: background 0.3s, color 0.3s; }
        body { background: var(--bg); color: var(--text); padding: 20px; min-height: 100vh; }
        .container { max-width: 1200px; margin: 0 auto; }

        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; }
        .brand { font-size: 1.6rem; font-weight: 800; color: var(--green); display: flex; align-items: center; gap: 10px; }
        
        .header-actions { display: flex; align-items: center; gap: 12px; }
        .theme-btn { background: var(--card); border: 1px solid var(--border); color: var(--text); padding: 8px 14px; border-radius: 20px; cursor: pointer; font-weight: 600; display: flex; align-items: center; gap: 6px; box-shadow: var(--shadow); }
        .status-badge { background: rgba(16, 185, 129, 0.2); color: var(--green); padding: 6px 14px; border-radius: 20px; font-weight: 700; font-size: 0.85rem; }

        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }

        .card { background: var(--card); border: 1px solid var(--border); border-radius: var(--radius); padding: 20px; box-shadow: var(--shadow); display: flex; flex-direction: column; gap: 15px; }
        .card-title { font-weight: 700; font-size: 1.1rem; display: flex; justify-content: space-between; align-items: center; }

        .val-large { font-size: 2rem; font-weight: 800; margin: 5px 0; }
        .sub-text { font-size: 0.85rem; color: var(--sub); }

        .progress-bg { height: 8px; width: 100%; background: var(--border); border-radius: 4px; overflow: hidden; }
        .progress-fill { height: 100%; width: 0%; border-radius: 4px; transition: width 0.5s ease; }

        .chart-box { height: 120px; width: 100%; }

        /* --- Dual Range Slider Style --- */
        .range-wrapper { display: flex; flex-direction: column; gap: 8px; }
        .range-header { display: flex; justify-content: space-between; font-size: 0.85rem; font-weight: 600; color: var(--sub); }
        .range-header span.val { color: var(--green); font-weight: 700; }
        .dual-slider { position: relative; width: 100%; height: 6px; background: var(--border); border-radius: 3px; }
        .dual-slider .slider-track { position: absolute; height: 100%; background: var(--green); border-radius: 3px; }
        .dual-slider input[type="range"] {
            position: absolute; width: 100%; height: 6px; top: 0; background: none; pointer-events: none; -webkit-appearance: none; appearance: none;
        }
        .dual-slider input[type="range"]::-webkit-slider-thumb {
            height: 18px; width: 18px; border-radius: 50%; background: var(--text); border: 2px solid var(--green); pointer-events: auto; -webkit-appearance: none; cursor: pointer;
        }
        .dual-slider input[type="range"]::-moz-range-thumb {
            height: 18px; width: 18px; border-radius: 50%; background: var(--text); border: 2px solid var(--green); pointer-events: auto; cursor: pointer;
        }

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
            <div class="brand"><i class="fa-solid fa-seedling"></i> სათბური Pro</div>
            <div class="header-actions">
                <button class="theme-btn" onclick="toggleTheme()">
                    <i class="fa-solid fa-moon" id="tIcon"></i>
                    <span id="tText">მუქი</span>
                </button>
                <div class="status-badge">ONLINE</div>
            </div>
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

                <!-- ერთ ვიზუალში გაერთიანებული ზღვრები -->
                <div class="range-wrapper">
                    <div class="range-header">
                        <span>ტემპერატურის ზღვარი</span>
                        <span class="val"><span id="tempMinLabel">18</span>°C - <span id="tempMaxLabel">28</span>°C</span>
                    </div>
                    <div class="dual-slider">
                        <div class="slider-track" id="tempTrack"></div>
                        <input type="range" id="tempMin" min="0" max="50" value="18" oninput="slideDual('temp', 'Min')">
                        <input type="range" id="tempMax" min="0" max="50" value="28" oninput="slideDual('temp', 'Max')">
                    </div>
                </div>

                <div class="relay-box">
                    <span style="font-size: 0.85rem; font-weight: 600;">ჰაერის გათბობა (Relay 1)</span>
                    <label class="switch">
                        <input type="checkbox" id="heatingToggle" onclick="toggleHeating()">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>

            <!-- 2. ნიადაგის გათბობა (Relay 2) -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-fire-flame-curved" style="color: var(--purple);"></i> ნიადაგის გათბობა</span>
                    <span id="soilHeatBadge" class="badge off">კაბელი: OFF</span>
                </div>
                <div>
                    <div class="sub-text">ფესვების ტემპერატურა (DS18B20)</div>
                    <div id="soilTemp" class="val-large" style="color: var(--purple);">--°C</div>
                </div>

                <!-- ერთ ვიზუალში გაერთიანებული ზღვრები -->
                <div class="range-wrapper">
                    <div class="range-header">
                        <span>ტემპერატურის ზღვარი</span>
                        <span class="val"><span id="soilTempMinLabel">18</span>°C - <span id="soilTempMaxLabel">22</span>°C</span>
                    </div>
                    <div class="dual-slider">
                        <div class="slider-track" id="soilTempTrack"></div>
                        <input type="range" id="soilTempMin" min="0" max="40" value="18" oninput="slideDual('soilTemp', 'Min')">
                        <input type="range" id="soilTempMax" min="0" max="40" value="22" oninput="slideDual('soilTemp', 'Max')">
                    </div>
                </div>

                <div class="relay-box">
                    <span style="font-size: 0.85rem; font-weight: 600;">გამათბობელი კაბელი (Relay 2)</span>
                    <label class="switch">
                        <input type="checkbox" id="soilHeatingToggle" onclick="toggleSoilHeating()">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>

            <!-- 3. ნიადაგის მორწყვა (Relay 3) -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-droplet" style="color: var(--amber);"></i> ნიადაგის მორწყვა</span>
                    <span id="waterBadge" class="badge off">მორწყვა: OFF</span>
                </div>
                <div>
                    <div class="sub-text">ნიადაგის ტენიანობა</div>
                    <div id="soilMoist" class="val-large" style="color: var(--amber);">--%</div>
                </div>
                <div class="chart-box"><canvas id="soilChart"></canvas></div>

                <!-- ერთ ვიზუალში გაერთიანებული ზღვრები -->
                <div class="range-wrapper">
                    <div class="range-header">
                        <span>ტენიანობის ზღვარი</span>
                        <span class="val"><span id="moistMinLabel">30</span>% - <span id="moistMaxLabel">70</span>%</span>
                    </div>
                    <div class="dual-slider">
                        <div class="slider-track" id="moistTrack"></div>
                        <input type="range" id="moistMin" min="0" max="100" value="30" oninput="slideDual('moist', 'Min')">
                        <input type="range" id="moistMax" min="0" max="100" value="70" oninput="slideDual('moist', 'Max')">
                    </div>
                </div>

                <div class="relay-box">
                    <span style="font-size: 0.85rem; font-weight: 600;">მორწყვის რელე (Relay 3)</span>
                    <label class="switch">
                        <input type="checkbox" id="wateringToggle" onclick="toggleWatering()">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>

            <!-- 4. განათება -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-sun" style="color: #eab308;"></i> განათება (BH1750)</span>
                </div>
                <div class="sub-text">სინათლის ინტენსივობა</div>
                <div id="lux" class="val-large" style="color: #eab308;">-- Lux</div>
                <div class="progress-bg"><div id="luxBar" class="progress-fill" style="background: #eab308;"></div></div>
            </div>

            <!-- 5. წყლის ავზი -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-faucet-drip" style="color: var(--blue);"></i> წყლის ავზი</span>
                </div>
                <div class="sub-text">ავზის შევსების დონე</div>
                <div id="waterLevel" class="val-large" style="color: var(--blue);">--%</div>
                <div class="progress-bg"><div id="waterBar" class="progress-fill" style="background: var(--blue);"></div></div>
            </div>

            <!-- 6. გარემო & უსაფრთხოება -->
            <div class="card">
                <div class="card-title">
                    <span><i class="fa-solid fa-shield-halved" style="color: var(--red);"></i> გარემო & უსაფრთხოება</span>
                </div>
                <div style="display: flex; justify-content: space-between;">
                    <div>
                        <div class="sub-text">ნალექი</div>
                        <div id="rainStatus" class="val-large" style="font-size: 1.3rem; color: var(--sub);">--</div>
                    </div>
                    <div>
                        <div class="sub-text">გაზი/კვამლი</div>
                        <div id="gasStatus" class="val-large" style="font-size: 1.3rem; color: var(--green);">ნორმა</div>
                    </div>
                </div>
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
                { label: 'SM', data: [], borderColor: '#f59e0b', fill: false, tension: 0.3 }
            ]},
            options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { display: false } } }
        });

        function slideDual(prefix, changed) {
            let minEl = document.getElementById(prefix + 'Min');
            let maxEl = document.getElementById(prefix + 'Max');
            let minVal = parseInt(minEl.value);
            let maxVal = parseInt(maxEl.value);

            if (minVal >= maxVal) {
                if (changed === 'Min') minEl.value = maxVal - 1;
                else maxEl.value = minVal + 1;
            }

            document.getElementById(prefix + 'MinLabel').innerText = minEl.value;
            document.getElementById(prefix + 'MaxLabel').innerText = maxEl.value;

            // Track Fill Update
            let track = document.getElementById(prefix + 'Track');
            let maxRange = parseInt(minEl.max);
            let leftPct = (minEl.value / maxRange) * 100;
            let rightPct = 100 - ((maxEl.value / maxRange) * 100);
            track.style.left = leftPct + '%';
            track.style.right = rightPct + '%';
        }

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
                    updateBadge("soilHeatBadge", data.soilHeatingState, "კაბელი");
                    updateBadge("waterBadge", data.wateringState, "მორწყვა");

                    // 1. ჰაერის გათბობა
                    var tUpper = parseFloat(document.getElementById("tempMax").value);
                    var tLower = parseFloat(document.getElementById("tempMin").value);
                    var hToggle = document.getElementById("heatingToggle");
                    if (data.airTemp > tUpper && hToggle.checked) { hToggle.checked = false; toggleHeating(); }
                    else if (data.airTemp < tLower && !hToggle.checked) { hToggle.checked = true; toggleHeating(); }

                    // 2. ნიადაგის გათბობის კაბელი (Relay 2)
                    var stUpper = parseFloat(document.getElementById("soilTempMax").value);
                    var stLower = parseFloat(document.getElementById("soilTempMin").value);
                    var shToggle = document.getElementById("soilHeatingToggle");
                    if (data.soilTemp > stUpper && shToggle.checked) { shToggle.checked = false; toggleSoilHeating(); }
                    else if (data.soilTemp < stLower && !shToggle.checked) { shToggle.checked = true; toggleSoilHeating(); }

                    // 3. ნიადაგის მორწყვა (Relay 3)
                    var mUpper = parseFloat(document.getElementById("moistMax").value);
                    var mLower = parseFloat(document.getElementById("moistMin").value);
                    var wToggle = document.getElementById("wateringToggle");
                    if (data.waterLevel < 10 && wToggle.checked) {
                        wToggle.checked = false;
                        toggleWatering();
                    } else {
                        if (data.soilMoist > mUpper && wToggle.checked) { wToggle.checked = false; toggleWatering(); }
                        else if (data.soilMoist < mLower && !wToggle.checked) { wToggle.checked = true; toggleWatering(); }
                    }

                    // გრაფიკები
                    const timeStr = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
                    if (labels.length >= maxPoints) labels.shift();
                    labels.push(timeStr);

                    if (climateChart.data.datasets[0].data.length >= maxPoints) {
                        climateChart.data.datasets[0].data.shift();
                        climateChart.data.datasets[1].data.shift();
                        soilChart.data.datasets[0].data.shift();
                    }

                    climateChart.data.datasets[0].data.push(data.airTemp);
                    climateChart.data.datasets[1].data.push(data.airHum);
                    soilChart.data.datasets[0].data.push(data.soilMoist);

                    climateChart.update();
                    soilChart.update();
                }
            };
            xhr.open("GET", "/readings?t=" + new Date().getTime(), true);
            xhr.send();
        }

        function updateBadge(id, state, text) {
            var el = document.getElementById(id);
            el.innerText = text + (state ? ": ON" : ": OFF");
            el.className = "badge " + (state ? "on" : "off");
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

        function toggleHeating() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleHeating", true);
            xhr.send();
        }

        function toggleSoilHeating() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleSoilHeating", true);
            xhr.send();
        }

        function toggleWatering() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleWatering", true);
            xhr.send();
        }

        slideDual('temp', 'Init');
        slideDual('soilTemp', 'Init');
        slideDual('moist', 'Init');
        loadTheme();
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
    pinMode(relayPinSoil, OUTPUT);
    pinMode(relayPinWater, OUTPUT);

    Wire.begin(D2, D3);
    
    if (!ads.begin()) {
        Serial.println("ADS1115 ვერ მოიძებნა!");
    }
    
    lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
    soilTempSensor.begin();

    EEPROM.begin(EEPROM_SIZE);
    uint8_t r1 = EEPROM.read(RELAY1_ADDR);
    uint8_t r2 = EEPROM.read(RELAY_SOIL_ADDR);
    uint8_t r3 = EEPROM.read(RELAY_WATER_ADDR);
    
    relayState1 = (r1 == 1);
    relayStateSoil = (r2 == 1);
    relayStateWater = (r3 == 1);

    digitalWrite(relayPin1, relayState1 ? HIGH : LOW);
    digitalWrite(relayPinSoil, relayStateSoil ? HIGH : LOW);
    digitalWrite(relayPinWater, relayStateWater ? HIGH : LOW);

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
        response += "\"soilHeatingState\":" + String(relayStateSoil ? "true" : "false") + ",";
        response += "\"wateringState\":" + String(relayStateWater ? "true" : "false");
        response += "}";

        request->send(200, "application/json", response);
    });

    server.on("/toggleHeating", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayState1 = !relayState1;
        digitalWrite(relayPin1, relayState1 ? HIGH : LOW);
        saveRelayState(RELAY1_ADDR, relayState1);
        request->send(200, "application/json", "{\"status\":\"OK\"}");
    });

    server.on("/toggleSoilHeating", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayStateSoil = !relayStateSoil;
        digitalWrite(relayPinSoil, relayStateSoil ? HIGH : LOW);
        saveRelayState(RELAY_SOIL_ADDR, relayStateSoil);
        request->send(200, "application/json", "{\"status\":\"OK\"}");
    });

    server.on("/toggleWatering", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayStateWater = !relayStateWater;
        digitalWrite(relayPinWater, relayStateWater ? HIGH : LOW);
        saveRelayState(RELAY_WATER_ADDR, relayStateWater);
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