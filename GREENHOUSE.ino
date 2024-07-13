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

const int relayPin1 = D2; // First relay pin
const int relayPin2 = D3; // Second relay pin
const int soilMoisturePin = A0; // Soil moisture sensor pin
bool relayState1 = true; // Initial state for the first relay
bool relayState2 = true; // Initial state for the second relay
int t = 0;
int h = 0;
const int AirValue = 667;   //you need to replace this value with Value_1
const int WaterValue = 280;  //you need to replace this value with Value_2
int soilMoistureValue = 0;
int soilmoisturepercent=0;

// Define EEPROM addresses for relay states
#define EEPROM_SIZE 2 // Total size of EEPROM
#define RELAY1_ADDR 0   // Address to store relay 1 state
#define RELAY2_ADDR 1   // Address to store relay 2 state
void saveRelayState(int addr, bool state); // Function declaration
// Set your Static IP address
IPAddress local_IP(192, 168, 1, 101);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
AsyncWebServer server(80);

const char* htmlPage = R"=====(
<!DOCTYPE html>
<html lang="ka">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="description" content="Greenhouse dashboard for monitoring and controlling temperature, soil moisture, and air humidity.">
    <title>Greenhouse</title>
    <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            background-color: #f9fbe7;
            color: #37474f;
            position: relative;
        }
        .container {
            text-align: center;
            padding: 10px;
        }
        h1 {
            font-size: 2rem;
            margin-bottom: 20px;
            color: #00b436;
        }
        .sensor-container {
            display: flex;
            justify-content: top;
            gap: 10px;
            margin-top: 10px;
        }
        .sensor-item {
            width: calc(33.33% - 20px);
            padding: 20px;
            background-color: #c7c7c7;
            border-radius: 10px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            text-align: center;
            transition: background-color 0.3s;
        }
        .sensor-item:hover {
            background-color: #d7d7d7;
        }
        .sensor-item {
            width: calc(50% - 20px); /* Adjust width to fit both items */
            padding: 20px;
            background-color: #c7c7c7;
            border-radius: 10px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            text-align: center;
            transition: background-color 0.3s;
            position: relative; /* Ensure relative positioning for child elements */
        }

        .sensor-value-container {
            display: flex;
            justify-content: space-around;
            margin-top: 20px;
        }

        .sensor-value {
            width: 100px; /* Adjust width as needed */
            text-align: center;
            font-size: 1.5rem;
            position: relative; /* Ensure relative positioning for absolute positioning of temperature and humidity */
        }
        .sensor-value:nth-child(2) {
            color: #1e88e5; /* Adjust color for humidity */
        }
        .sensor-value:before {
            content: '';
            position: absolute;
            left: 50%; /* Adjust position as needed */
            transform: translateX(-50%);
            bottom: -10px; /* Adjust spacing between readings */
            height: 10px;
            width: 1px;
             /* Adjust color for separator */
        }
        .switch-container {
            display: inline-block;
            vertical-align: middle;
            margin-top: 20px;
        }
        .switch-container input[type="checkbox"] {
            display: none;
        }
        .switch-slider {
            position: relative;
            display: inline-block;
            width: 50px;
            height: 24px;
            border-radius: 34px;
            transition: background-color 0.4s;
            cursor: pointer;
            background-color: var(--switch-off-color);
        }
        .switch-slider:before {
            content: "";
            position: absolute;
            height: 20px;
            width: 20px;
            left: 2px;
            bottom: 2px;
            background-color: #ffffff;
            border-radius: 50%;
            transition: transform 0.4s;
        }
        .switch-container input[type="checkbox"]:checked + .switch-slider {
            background-color: var(--switch-on-color);
        }
        .switch-container input[type="checkbox"]:checked + .switch-slider:before {
            transform: translateX(26px);
        }
        .relay-button {
            padding: 10px 20px;
            font-size: 1rem;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            margin: 0 10px;
            transition: background-color 0.3s, color 0.3s, transform 0.2s;
            background-color: #4caf50;
            color: white;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
        }
        .relay-button:hover {
            background-color: #388e3c;
            transform: translateY(-2px);
        }
        .relay-button:focus {
            outline: none;
        }
        .input-graph {
            margin-top: 10px;
            position: relative;
        }
        .range-dot {
            position: absolute;
            height: 10px;
            width: 10px;
            background-color: #000;
            border-radius: 50%;
            transform: translateX(-50%);
        }
        .lower-dot {
            top: 0;
        }
        .upper-dot {
            top: 0;
            right: 0;
        }
        .sensor-item {
            position: relative;
        }
        #temperatureSensor .range-dot.lower-dot,
        #moistureSensor .range-dot.lower-dot,
        #humiditySensor .range-dot.lower-dot {
            --position: calc((var(--position) / (var(--max) - var(--min))) * 100%);
            left: var(--position);
        }
        #temperatureSensor .range-dot.lower-dot {
            background-color: #3f51b5;
        }
        #moistureSensor .range-dot.lower-dot {
            background-color: #8bc34a;
        }
        #humiditySensor .range-dot.lower-dot {
            background-color: #ff9800;
        }
        body.dark-mode {
            background-color: #303030;
            color: #e0e0e0;
        }
        .dark-mode .container {
            background-color: #424242;
        }
        .dark-mode h1 {
            color: #81d4fa;
        }
        .dark-mode .sensor-item {
            background-color: #616161;
            color: #e0e0e0;
        }
        .dark-mode .sensor-value {
            color: #81d4fa;
        }
        .dark-mode .sensor-item p {
            color: #81d4fa;
        }
        .dark-mode .switch-slider {
            background-color: #595656;
        }
        .dark-mode .switch-slider:before {
            background-color: #757575;
        }
        .dark-mode .relay-button {
            background-color: #81d4fa;
        }
        .dark-mode .relay-button:hover {
            background-color: #4caf50;
        }
        #darkModeSwitchContainer {
            position: absolute;
            top: 10px;
            right: 10px;
        }
    </style>
</head>
<body>
    <div id="darkModeSwitchContainer" class="switch-container">
        <label class="switch-container" style="--switch-on-color: #030303; --switch-off-color: #bdbdbd;">
            <input type="checkbox" id="darkModeToggle" onclick="toggleDarkMode()">
            <span class="switch-slider"></span>
        </label>
    </div>
    <div class="container">
        <h1>სათბური</h1>
        <div class="sensor-container">
            <div class="sensor-item" id="temperatureHumiditySensor">
                <p>ტემპერატურა და ჰაერის ტენიანობა</p>
                <div class="sensor-value-container">
                    <div class="sensor-value" style="color: #e90808;">
                        <span id="temp">--</span>
                    </div>
                    <div class="sensor-value" style="color: #1e88e5;">
                        <span id="humid">--</span>
                    </div>
                </div>
                <input type="range" id="tempLowerLimit" class="input-graph" min="0" max="50" value="0" oninput="updateLimitDisplay('temp')">
                <p>Lower Limit: <span id="tempLowerLimitDisplay">0°C</span></p>
                <input type="range" id="tempLimit" class="input-graph" min="0" max="50" value="0" oninput="updateLimitDisplay('temp')">
                <p>Upper Limit: <span id="tempLimitDisplay">0°C</span></p>
                <label class="switch-container" style="--switch-on-color: #e90808; --switch-off-color: #bdbdbd;">
                    <input type="checkbox" id="heatingToggle" onclick="toggleHeating()">
                    <span class="switch-slider"></span>
                </label>
            </div>
            <div class="sensor-item" id="moistureSensor">
                <p>ნიადაგის ტენიანობა</p>
                <div class="sensor-value-container">
                    <div class="sensor-value" style="color: #713c2a;">
                        <span id="moisture">--</span>
                    </div>
                </div>
                <input type="range" id="moistureLowerLimit" class="input-graph" min="0" max="100" value="0" oninput="updateLimitDisplay('moisture')">
                <p>Lower Limit: <span id="moistureLowerLimitDisplay">0%</span></p>
                <input type="range" id="moistureLimit" class="input-graph" min="0" max="100" value="0" oninput="updateLimitDisplay('moisture')">
                <p>Upper Limit: <span id="moistureLimitDisplay">0%</span></p>
                <label class="switch-container" style="--switch-on-color: #713c2a; --switch-off-color: #bdbdbd;">
                    <input type="checkbox" id="wateringToggle" onclick="toggleWatering()">
                    <span class="switch-slider"></span>
                </label>
            </div>
        </div>
        
    <script>
        function updateReadings() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (xhr.readyState == XMLHttpRequest.DONE && xhr.status == 200) {
                    var response = JSON.parse(xhr.responseText);
                    var tempSymbol = '\u00B0C';
                    document.getElementById("temp").innerText = response.temperature + tempSymbol;
                    document.getElementById("moisture").innerText = response.moisture + "%";
                    document.getElementById("humid").innerText = response.humidity + " %";

                    var tempLimit = parseFloat(document.getElementById("tempLimit").value);
                    var tempLowerLimit = parseFloat(document.getElementById("tempLowerLimit").value);
                    var moistureLimit = parseFloat(document.getElementById("moistureLimit").value);
                    var moistureLowerLimit = parseFloat(document.getElementById("moistureLowerLimit").value);

                    var heatingToggle = document.getElementById("heatingToggle");
                    var wateringToggle = document.getElementById("wateringToggle");

                    if (response.temperature > tempLimit && heatingToggle.checked) {
                        heatingToggle.checked = false;
                        toggleHeating(false);
                    } else if (response.temperature < tempLowerLimit && !heatingToggle.checked) {
                        heatingToggle.checked = true;
                        toggleHeating(false);
                    } else if (response.temperature >= tempLowerLimit && response.temperature <= tempLimit && !heatingToggle.checked) {
                        heatingToggle.checked = true;
                        toggleHeating(false);
                    }

                    if (response.moisture > moistureLimit && wateringToggle.checked) {
                        wateringToggle.checked = false;
                        toggleWatering(false);
                    } else if (response.moisture < moistureLowerLimit && !wateringToggle.checked) {
                        wateringToggle.checked = true;
                        toggleWatering(false);
                    } else if (response.moisture >= moistureLowerLimit && response.moisture <= moistureLimit && !wateringToggle.checked) {
                        wateringToggle.checked = true;
                        toggleWatering(false);
                    }
                }
            };
            xhr.open("GET", "/readings", true);
            xhr.send();
        }

        function updateLimitDisplay(sensor) {
            var lowerLimit = document.getElementById(sensor + 'LowerLimit').value;
            var upperLimit = document.getElementById(sensor + 'Limit').value;
            var lowerDisplay = document.getElementById(sensor + 'LowerLimitDisplay');
            var upperDisplay = document.getElementById(sensor + 'LimitDisplay');

            if (sensor === 'temp') {
                lowerDisplay.innerText = lowerLimit + '°C';
                upperDisplay.innerText = upperLimit + '°C';
            } else {
                lowerDisplay.innerText = lowerLimit + '%';
                upperDisplay.innerText = upperLimit + '%';
            }
        }

        function toggleDarkMode() {
            document.body.classList.toggle('dark-mode');
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
            var heatingToggle = document.getElementById("heatingToggle");
            var wateringToggle = document.getElementById("wateringToggle");
            var heatingEnabled = localStorage.getItem("heatingEnabled");
            var wateringEnabled = localStorage.getItem("wateringEnabled");
            if (heatingEnabled !== null) {
                heatingToggle.checked = heatingEnabled === "true";
            }
            if (wateringEnabled !== null) {
                wateringToggle.checked = wateringEnabled === "true";
            }
        }

        applyToggleState();
        setInterval(updateReadings, 2000);
    </script>
</body>
</html>

)=====";

// Initialize variables for sensor readings
unsigned long previousMillis = 0; // Variable to store the last time DHT values were updated
const long interval = 2000; // Interval at which to update DHT values (in milliseconds)

void setup() {
    Serial.begin(115200);
    pinMode(DHTPIN, INPUT);
    pinMode(relayPin1, OUTPUT);
    pinMode(relayPin2, OUTPUT);
    pinMode(soilMoisturePin, INPUT);

    // Initialize EEPROM with predefined size
    EEPROM.begin(EEPROM_SIZE);
        // Read saved relay states from EEPROM
    relayState1 = EEPROM.read(RELAY1_ADDR);
    relayState2 = EEPROM.read(RELAY2_ADDR);
    // Set relay pins according to the saved states
    digitalWrite(relayPin1, relayState1 ? HIGH : LOW);
    digitalWrite(relayPin2, relayState2 ? HIGH : LOW);

   // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting...");
    }
    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP());

    dht.begin();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", htmlPage);
    });

    server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();
        int moisture = analogRead(soilMoisturePin);

        if (isnan(temperature) || isnan(humidity)) {
            request->send(500, "text/plain", "Failed to read from DHT sensor");
            return;
        }

        // Update relay state based on client connection
        String response = "{\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity) + ",\"moisture\":" + String(map(soilMoistureValue, AirValue, WaterValue, 0, 100)) + ",\"heatingState\":" + String(relayState1) + ",\"wateringState\":" + String(relayState2) + "}";
        request->send(200, "application/json", response);
    });

    server.on("/toggleHeating", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayState1 = !relayState1;
        digitalWrite(relayPin1, relayState1 ? HIGH : LOW);
        saveRelayState(RELAY1_ADDR, relayState1);

        String status = relayState1 ? "OFF" : "ON";
        String response = "{\"status\":\"" + status + "\"}";
        request->send(200, "application/json", response);
    });

    server.on("/toggleWatering", HTTP_GET, [](AsyncWebServerRequest *request) {
        relayState2 = !relayState2;
        digitalWrite(relayPin2, relayState2 ? HIGH : LOW);
        saveRelayState(RELAY2_ADDR, relayState2);

        String status = relayState2 ? "OFF" : "ON";
        String response = "{\"status\":\"" + status + "\"}";
        request->send(200, "application/json", response);
    });

    server.begin();
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        // Read temperature as Celsius (the default)
        float newT = dht.readTemperature();
        if (!isnan(newT)) {
            t = newT;
            Serial.println("Temperature: " + String(t));
        } else {
            Serial.println("Failed to read temperature from DHT sensor!");
        }

        // Read humidity
        float newH = dht.readHumidity();
        if (!isnan(newH)) {
            h = newH;
            Serial.println("Humidity: " + String(h));
        } else {
            Serial.println("Failed to read humidity from DHT sensor!");
        }

        // Read soil moisture
        soilMoistureValue = analogRead(soilMoisturePin);  //put Sensor insert into soil
        soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
        Serial.println("Soil Moisture: " + String(soilmoisturepercent));
    }
}
// Function to save relay state to EEPROM
void saveRelayState(int addr, bool state) {
    EEPROM.write(addr, state);
    EEPROM.commit();
}