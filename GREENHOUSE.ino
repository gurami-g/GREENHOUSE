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
IPAddress local_IP(192, 168, 0, 101);
// Set your Gateway IP address
IPAddress gateway(192, 168, 0, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
AsyncWebServer server(80);

const char* htmlPage = R"=====(
<!DOCTYPE html>
<html lang="ka">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Greenhouse</title>
    <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
    <style>
body {
    font-family: Arial, sans-serif;
    margin: 0;
    padding: 0;
    background-color: #f9fbe7; /* Light yellow background */
    color: #37474f; /* Dark grayish-blue text */
    position: relative;
}

.container {
    text-align: center;
    padding: 20px;
}

h1 {
    font-size: 2rem;
    margin-bottom: 20px;
    color: #00b436; /* Green */
}

.sensor-container {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    gap: 20px;
    margin-top: 20px;
}

.sensor-item {
    width: calc(25% - 20px);
    padding: 20px;
    background-color: #c7c7c7; /* Light gray */
    border-radius: 10px;
    box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
    text-align: center;
}

.sensor-item p {
    margin-top: 10px;
    font-size: 1rem;
    color: #00b436; /* Green */
}

.sensor-value-container {
    position: relative;
    width: 100px;
    height: 100px;
    border-radius: 50%;
    display: flex;
    justify-content: center;
    align-items: center;
    overflow: hidden;
    background-color: #f5f5f5; 
}

.sensor-value {
    position: absolute;
    top: 50%;
    left: 0;
    width: 100%;
    text-align: center;
    transform: translateY(-50%);
    font-size: 1.5rem;
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
}

.switch-slider:before {
    content: "";
    position: absolute;
    height: 20px;
    width: 20px;
    left: 2px;
    bottom: 2px;
    background-color: #ffffff; /* White */
    border-radius: 50%;
    transition: transform 0.4s;
}

.switch-container input[type="checkbox"]:checked + .switch-slider {
    background-color: var(--switch-on-color);
}

.switch-container input[type="checkbox"] + .switch-slider {
    background-color: var(--switch-off-color);
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
    background-color: #4caf50; /* Green */
    color: white;
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}

.relay-button:hover {
    background-color: #388e3c; /* Darker green */
    transform: translateY(-2px);
}

.relay-button:focus {
    outline: none;
}

/* Dark Mode Styles */
body.dark-mode {
    background-color: #303030; /* Dark gray */
    color: #e0e0e0; /* Light gray */
}

.dark-mode .container {
    background-color: #424242; /* Dark gray */
}

.dark-mode h1 {
    color: #81d4fa; /* Light blue */
}

.dark-mode .sensor-item {
    background-color: #616161; /* Medium gray */
    color: #e0e0e0; /* Light gray */
}

.dark-mode .sensor-value-container {
    background-color: #757575; /* Darker gray */
}

.dark-mode .sensor-value {
    color: #81d4fa; /* Light blue */
}

.dark-mode .sensor-item p {
    color: #81d4fa; /* Light blue */
}

.dark-mode .switch-slider {
    background-color: #595656; /* Light gray */
}

.dark-mode .switch-slider:before {
    background-color: #757575; /* Darker gray */
}

.dark-mode .relay-button {
    background-color: #81d4fa; /* Light blue */
}

.dark-mode .relay-button:hover {
    background-color: #4caf50; /* Green */
}

/* Position Dark Mode Switch in the Top Right Corner */
#darkModeSwitchContainer {
    position: absolute;
    top: 10px;
    right: 10px;
}
    </style>
</head>
<body>
    <!-- Dark Mode Switch -->
    <div id="darkModeSwitchContainer" class="switch-container">
        <label class="switch-container" style="--switch-on-color: #030303; --switch-off-color: #bdbdbd;">
            <input type="checkbox" id="darkModeToggle" onclick="toggleDarkMode()">
            <span class="switch-slider"></span>
        </label>
    </div>
    <div class="container">
        <h1>სათბური</h1>
        <!-- Sensor Containers -->
        <div class="sensor-container">
            <div class="sensor-item" id="temperatureSensor">
                <p>ტემპერატურა</p>
                <div class="sensor-value-container">
                    <div class="sensor-value" style="color: #e90808;"> <!-- Red for temperature -->
                        <span id="temp">--</span>
                    </div>
                </div>
                <label class="switch-container" style="--switch-on-color: #e90808; --switch-off-color: #bdbdbd;">
                    <input type="checkbox" id="heatingToggle" onclick="toggleHeating()">
                    <span class="switch-slider"></span>
                </label>
            </div>
            <div class="sensor-item" id="moistureSensor">
                <p>ნიადაგის ტენიანობა</p>
                <div class="sensor-value-container">
                    <div class="sensor-value" style="color: #713c2a;"> <!-- Brown for moisture -->
                        <span id="moisture">--</span>
                    </div>
                </div>
                <label class="switch-container" style="--switch-on-color: #713c2a; --switch-off-color: #bdbdbd;">
                    <input type="checkbox" id="wateringToggle" onclick="toggleWatering()">
                    <span class="switch-slider"></span>
                </label>
            </div>
            <div class="sensor-item" id="humiditySensor">
                <p>ჰაერის ტენიანობა</p>
                <div class="sensor-value-container">
                    <div class="sensor-value" style="color: #1e88e5;"> <!-- Blue for humidity -->
                        <span id="humid">--</span>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        // Function to update sensor readings
        function updateReadings() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (xhr.readyState == XMLHttpRequest.DONE && xhr.status == 200) {
                    var response = JSON.parse(xhr.responseText);
                    var tempSymbol = '\u00B0C';
                    // Update temperature reading
                    document.getElementById("temp").innerText = response.temperature + tempSymbol;
                    // Update moisture reading
                    document.getElementById("moisture").innerText = response.moisture + "%";
                    // Update humidity reading
                    document.getElementById("humid").innerText = response.humidity + " %";
                }
            };
            xhr.open("GET", "/readings", true);
            xhr.send();
        }

        // Function to toggle dark mode
        function toggleDarkMode() {
            document.body.classList.toggle('dark-mode');
        }

        // Function to toggle heating relay
        function toggleHeating() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleHeating", true);
            xhr.send();
            var heatingToggle = document.getElementById("heatingToggle");
            localStorage.setItem("heatingEnabled", heatingToggle.checked);
        }

        // Function to toggle watering relay
        function toggleWatering() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggleWatering", true);
            xhr.send();
            var wateringToggle = document.getElementById("wateringToggle");
            localStorage.setItem("wateringEnabled", wateringToggle.checked);
        }

        // Function to retrieve and apply the stored toggle state on page load
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
        Serial.println("Soil Moisture: " + String(soilmoisturepercent) + "%");
    }
}
// Function to save relay state to EEPROM
void saveRelayState(int addr, bool state) {
    EEPROM.write(addr, state);
    EEPROM.commit();
}