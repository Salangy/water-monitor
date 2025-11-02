#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Water Tank Management System for ESP8266
// Implements manual override and automatic filling with safety timeouts.

#define D1    5
#define D2    4
#define D3    0       //OUTPUT ONLY connected to FLASH button, boot fails if pulled LOW
#define D4    2       //OUTPUT ONLY HIGH at boot connected to on-board LED, boot fails if pulled LOW
#define D5    14
#define D6    12

const char* ssid = "NOKIA 2.4";         // Replace with your network SSID (name) NOKIA-9FE1
const char* password = "SjuLEqL6YB"; // Replace with your network password
const char* esp_hostname = "esphost"; // Desired hostname

// Variables to store the measurement
long duration; // Variable for the travel time of the sound pulse
float distanceCm; // Variable for the calculated distance in centimeters

// --- PIN DEFINITIONS (Adjust these based on your wiring) ---
//Ultrasonic is use for this sendor
const int LEVEL_LOW_SENSOR_PIN = D4;  // GPIO2 - Digital Output for the pump relay (HIGH = Pump ON)
//D2 conected directly to the realy pin on the board.
const int PUMP_RELAY_PIN  = D2;  // GPIO4 - Digital Input for the ultrasonic sensor (ECHO)
const int SENSOR_TRIGER_PIN = D3;  // GPIO0 - Digital Output for the ultrasonic sensor (TRIGER)
const int OVERFLOW_SENSOR_PIN = D1; // GPIO5 - Digital Input (HIGH when tank is FULL/overflowing)
const int SENSOR_ECHO_PIN = D5; // GPIO14 - Digital Input (HIGH when water is BELOW 50%)
const int FLOW_SENSOR_PIN = D6; // GPIO12 - Digital Input (HIGH when water flow is detected)

// Define the pins connected to the sensor
// const int TRIG_PIN = D1; // Corresponds to GPIO 5 on ESP8266 boards
// const int ECHO_PIN = D2; // Corresponds to GPIO 4 on ESP8266 boards

//SAFTY SHUTDOWN
bool saftyShutdwon = false;

bool toggle = false;

bool saftyMessage = false;

int waterLevelReading = 0;

// ---  AND STATE MANAGEMENT ---
enum Mode { AUTO_MODE, MANUAL_MODE };
Mode currentMode = AUTO_MODE;

// Define the two thresholds
const int HIGH_LEVEL_LIMIT = 180;
const int LOW_LEVEL_LIMIT = 100;


// State tracking
bool pumpIsRunning = false;
unsigned long pumpStartTime = 0;

// Safety and Time Constants
const unsigned long SAFETY_TIMEOUT_MS = 1 * 60 * 1000; // 5 minutes in milliseconds

// Simulated Inputs (For testing purposes, a real app would read these from Web UI/API/Buttons)
bool manualPumpOnRequest = false;
bool manualPumpOffRequest = false;

StaticJsonDocument<200> doc_rx;                    // create a JSON container

String jsonString = "";  
StaticJsonDocument<200> doc;
JsonObject object = doc.to<JsonObject>(); 

String webpage = "<!DOCTYPE html> <html> <head> <meta name='viewport' content='width=device-width, initial-scale=1' > <style> body{ font-family:sans-serif; display:flex; align-items:center; justify-content:right; min-height:100vh; background-color: #c6d8eb; } .water-tank { width:10em; height:30em; border:.3em solid #f7f9fb; border-top:none; box-sizing:border-box; position:relative; box-shadow: -1px 1px #6494b7, -2px 2px #6494b7, -3px 3px #6494b7, -4px 4px #6494b7, -5px 5px #6494b7; } .water-tank .liquid { width:100%; height:100%; position:absolute; overflow:hidden; } .water-tank .liquid svg { height:30em; /* top: calc(100% - 1%); */ position:absolute; animation: waves 5s infinite linear; } @keyframes waves { 0% { transform:translateX(-15em); } 100% { transform:translateX(0); } } .water-tank .label { position:absolute; color:white; line-height:2em; width:4em; text-align:center; border-radius:.5em; margin-bottom: -1em; background-color:#10c340; right:2.9em; /* bottom:8%; */ } .water-tank .indicator { position:absolute; background-color:#3A3A3A; height:0.3em; width:1em; margin-bottom: -0.15em; right:0; } .water-tank .indicator[data-value='25'] { bottom: 25%; background-color: red; } .water-tank .indicator[data-value='50'] { bottom: 50%; background-color: yellow; } .water-tank .indicator[data-value='75'] { bottom: 75%; background-color: green; } .main-container{ height: 30em; width: 100%; margin-left: 5px; } .left-container{ height: 30em; width: 50%; float: left; } .right-container{ height: 30em; margin-left: 52%; } .plate { width: 10em; height: 30em; background-color: #75afe6; /* Add a background color */ /* Add more CSS styles as needed to customize the appearance */ box-shadow: -1px 1px #6494b7, -2px 2px #6494b7, -3px 3px #6494b7, -4px 4px #6494b7, -5px 5px #6494b7; margin-left: auto; margin-right: 0; } .green { background-image: -webkit-linear-gradient(top, #13fB04 0%, #58e343 50%, #ADED99 100%); } .orange { background-image: -webkit-linear-gradient(top, #f9a004 0%, #e0ac45 50%, #ead698 100%); } .red { background-image: -webkit-linear-gradient(top, #fb1304 0%, #e35843 50%, #edad99 100%); } .led { margin-top: 5px; margin-bottom: 5px; margin-left: auto; margin-right: auto; border-radius: 5px; width: 5px; height: 5px; box-shadow: 0px 0px 3px black; zoom: 5; } .switch { position: relative; display: inline-block; width: 60px; height: 34px; } .switch input { opacity: 0; width: 0; height: 0; } .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; -webkit-transition: .4s; transition: .4s; } .slider:before { position: absolute; content: ''; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; -webkit-transition: .4s; transition: .4s; } input:checked + .slider { background-color: #2196F3; } input:focus + .slider { box-shadow: 0 0 1px #2196F3; } input:checked + .slider:before { -webkit-transform: translateX(26px); -ms-transform: translateX(26px); transform: translateX(26px); } /* Rounded sliders */ .slider.round { border-radius: 34px; } .slider.round:before { border-radius: 50%; } </style> </head> <body> <script> var Socket; var distance = 0; element = []; let toggle = 'false'; function init() { for(i=0;i<=450;i++){ element[i] = 450 - i; } document.getElementById('buttonToggle').addEventListener('click', onToggle); document.getElementById('manualEnable').addEventListener('click', onManual); document.getElementById('manualDisable').addEventListener('click', onManual); Socket = new WebSocket('ws://' + window.location.hostname + ':5000/ws'); Socket.onerror = function(event) { console.log('Connection Error'); document.getElementById('heartbeat').className = 'red led'; setTimeout(function() { init(); }, 2000); }; Socket.onopen = function(event) { console.log('Connection established'); document.getElementById('heartbeat').className = 'green led'; }; Socket.onmessage = function(event) { processCommand(event); }; Socket.onclose = function(event) { console.log('Connection closed'); document.getElementById('heartbeat').className = 'red led'; setTimeout(function() { init(); }, 2000); }; } function onToggle(event) { var buttonState = document.getElementById('buttonToggle').checked; Socket.send(JSON.stringify({'buttonState':buttonState})); } function onManual(event){ var manualState = document.getElementById('manualEnable').checked; Socket.send(JSON.stringify({'manualState':manualState})); } function processCommand(event) { var obj = JSON.parse(event.data); if( Math.ceil(obj.WATER_LEVEL) > 179){ distance = 179; }else{ distance = obj.WATER_LEVEL; } var svg = document.getElementById('xyz'); var lbl = document.getElementById('wlbl'); var pixel = Math.ceil(((distance - 20) * 450) / 160); var percent = Math.ceil(((distance - 20) * 100) / 160); if(distance == 0.00){ percent = 5; pixel = 0; } svg.style['top']= element[pixel] + 'px'; lbl.innerHTML = percent + '%'; lbl.style['bottom'] = percent + '%'; if(obj.AUTO_MODE == 'ON'){ document.getElementById('buttonToggle').checked = false; } if(obj.OVER_FLOW == 'ON'){ document.getElementById('overflow-sensor').className ='red led'; }else if(obj.OVER_FLOW == 'OFF'){ document.getElementById('overflow-sensor').className ='green led'; } if(obj.FLOW == 'ON'){ document.getElementById('flow-sensor').className ='red led'; }else if(obj.FLOW == 'OFF'){ document.getElementById('flow-sensor').className ='green led'; } if(obj.PUMP == 'ON'){ document.getElementById('water-pump').className ='red led'; }else if(obj.PUMP == 'OFF'){ document.getElementById('water-pump').className ='green led'; } var obj = JSON.parse(event.data); console.log(obj.WATER_LEVEL); if(obj.TOGGLE == true){ document.getElementById('heartbeat').className ='green led'; }else{ document.getElementById('heartbeat').className ='red led'; } if(obj.SAFTY_OFF == true){ console.log('safty activated.'); document.getElementById('safty-shut-down').textContent  = 'Safty shut down.'; } } window.onload = function(event) { init(); } </script> <div class='main-container'> <div style='height: 2em;font-size: xx-large; font-family:Franklin Gothic Medium, Arial Narrow, Arial, sans-serif; text-align: center;'> Water Tank Monitor </div> <div id='safty-shut-down'></div> <div> <input type='radio' id='manualEnable' name='manual'> <label for='manualEnable'>Enable</label><br> <input type='radio' id='manualDisable' name='manual'checked> <label for='manualDisable'>Disable</label><br> </div> <div class='left-container' > <div class='plate' > <div style='text-align: center; padding-top: 30px;'> Flow Sensor </div> <div class='green led' id='flow-sensor'></div> <div style='text-align: center;'> Overflow Sensor </div> <div class='green led' id='overflow-sensor'></div> <div style='text-align: center;'> Water Pump </div> <div class='green led' id='water-pump'></div> <div style='text-align: center;'> Heartbeat </div> <div class='green led' id='heartbeat'></div> <div style='text-align: center; width: 100%; padding-top:2rem;'> <label class='switch'> <input type='checkbox' id='buttonToggle'> <span class='slider round'></span> </label> </div> </div> </div> <div class='right-container'> <div class='water-tank'> <div class='liquid' > <svg class='water' viewBox='0 0 200 100' id='xyz'> <defs> <linearGradient id='waterGradient' x1='0%' y1='0%' x2='0%' y2='100%'> <stop offset='0' style='stop-color:#29ABE2'/> <stop offset='0.1643' style='stop-color:#28A6E3'/> <stop offset='0.3574' style='stop-color:#2496E6'/> <stop offset='0.5431' style='stop-color:#1E7DEA'/> <stop offset='0.7168' style='stop-color:#1559F0'/> <stop offset='0.874' style='stop-color:#0B2CF7'/> <stop offset='1' style='stop-color:#0000FF'/> </linearGradient> </defs> <path fill='url(#waterGradient)' d=' M 0,0 v 100 h 200 v -100 c -10,0 -15,5 -25,5 c -10,0 -15,-5 -25,-5 c -10,0 -15,5 -25,5 c -10,0 -15,-5 -25,-5 c -10,0 -15,5 -25,5 c -10,0 -15,-5 -25,-5 c -10,0 -15,5 -25,5 c -10,0 -15,-5 -25,-5 '/> </svg> </div> <div class='indicator' data-value='75'></div> <div class='indicator' data-value='50'></div> <div class='indicator' data-value='25'></div> <div class='label' id='wlbl'></div> </div> </div> </div> </body> </html>";
// Initialization of webserver and websocket

//Server port and WebSocket in html should be the same to work properly.
//Socket = new WebSocket('ws://' + window.location.hostname + ':5000/ws');

AsyncWebServer server(5000);
AsyncWebSocket ws("/ws");

uint32_t messageId;   

// Forward declaration of webSocketEvent
//void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length);


// --- FUNCTION DEFINITIONS ---

/**
 * Sets the physical state of the pump and updates internal state tracking.
 * @param newState true for ON, false for OFF.
 * @param reason A string describing why the state was changed (for Serial logging).
 */
void setPumpState(bool newState, const char* reason) {
  if (pumpIsRunning != newState) {
    pumpIsRunning = newState;
    digitalWrite(PUMP_RELAY_PIN, pumpIsRunning ? HIGH : LOW);

    Serial.print("PUMP STATE CHANGE: ");
    Serial.print(pumpIsRunning ? "ON" : "OFF");
    Serial.print(" | Reason: ");
    Serial.println(reason);

    if(pumpIsRunning){
      object["PUMP"] = "ON";
    }else{
       object["PUMP"] = "OFF";
    }

    serializeJson(doc,jsonString);
    ws.textAll(jsonString);

    // If we just turned the pump ON, record the start time for the safety timeout.
    if (pumpIsRunning) {
      pumpStartTime = millis();
    }
  }
}

/**
 * Reads all digital sensor inputs.
 */
struct SensorReadings {
  bool isOverflowActive;
  bool isLevelLow;
  bool isFlowActive;
};

float GetUltrasonicSensorReading(){

  // 1. CLEAR THE TRIG PIN
  // Ensure the trigger pin is low before sending a pulse
  digitalWrite(SENSOR_TRIGER_PIN, LOW);
  delayMicroseconds(2); // Wait a short period for stability

  // 2. TRIGGER THE SENSOR
  // Send a 10 microsecond pulse to the TRIG pin
  digitalWrite(SENSOR_TRIGER_PIN, HIGH);
  delayMicroseconds(10); 
  digitalWrite(SENSOR_TRIGER_PIN, LOW);

  // 3. MEASURE THE PULSE DURATION
  // 'pulseIn' reads the ECHO pin: returns the duration (in microseconds) 
  // of the high pulse. This is the time the sound took to travel to the 
  // object and back.
  // It waits for the pin to go HIGH, starts timing, waits for the pin to go LOW, and stops timing.
  duration = pulseIn(SENSOR_ECHO_PIN, HIGH);

  // 4. CALCULATE THE DISTANCE (cm)
  /*
    The formula for distance: Distance = (Duration * Speed of Sound) / 2
    
    Speed of Sound is approx 343 m/s, or 0.0343 cm/µs.
    Distance (cm) = Duration (µs) * 0.0343 / 2
    Distance (cm) = Duration (µs) / 58.23 
    (We use 58.0 for simplicity and common usage, but 58.23 is more precise)
  */
  distanceCm = duration / 58.0;

  // 5. OUTPUT RESULTS
  
  if (duration == 0) {
    Serial.println("Error: Sensor timeout or no object detected.");
  } else {
      // Check for sensible range (usually < 400cm)
      if (distanceCm >= 400 || distanceCm <= 0) {
        Serial.print("Out of Range: ");
      } else {
        Serial.print("Distance: ");
        Serial.print(distanceCm);
      }
      
      Serial.println(" cm"); 
  }
  return distanceCm;
}
int getLevelReading(){
  int reading =(int)GetUltrasonicSensorReading();
  waterLevelReading = reading;
  int logic = 0;
  if(reading > HIGH_LEVEL_LIMIT){
    logic = 0;
  }else if(reading < LOW_LEVEL_LIMIT){
    logic = 1;
  }else{
    if(pumpIsRunning){
      logic = 1;
    }else{
      logic = 0;
    }

  }  
  return logic;
}


SensorReadings readSensors() {
  SensorReadings sensors;
  // Assume sensors are wired pull-up/pull-down appropriately.
  // We use digitalRead() here; adjust the logic (e.g., HIGH/LOW) based on your sensor wiring.
  sensors.isOverflowActive = digitalRead(OVERFLOW_SENSOR_PIN) == HIGH; // Full tank = HIGH
  sensors.isLevelLow = getLevelReading();     //digitalRead(LEVEL_LOW_SENSOR_PIN) == HIGH; // < 50% water = HIGH
  sensors.isFlowActive = digitalRead(FLOW_SENSOR_PIN) == HIGH; // Flow detected = HIGH
  return sensors;
}

/**
 * The main logic implementing the provided pseudo-code.
 */
void manageTank(const SensorReadings& sensors) {

  // --- MANUAL MODE LOGIC ---
  if (currentMode == MANUAL_MODE) {
    if (manualPumpOnRequest) {
      setPumpState(true, "Manual ON request");
      // Reset requests after use
      manualPumpOnRequest = false;
      manualPumpOffRequest = false;
    } else if (manualPumpOffRequest) {
      setPumpState(false, "Manual OFF request");
      // Reset requests after use
      manualPumpOnRequest = false;
      manualPumpOffRequest = false;
    }
    // If no explicit manual command, pump state remains unchanged until the next command.
  }
  
  // --- AUTOMATIC MODE LOGIC ---
  else { // currentMode == AUTO_MODE
    
    // 1. Check for Overflow (Highest Priority Stop)
    if (sensors.isOverflowActive) {
      // if(over flow sensor true) { Turn off the pump }
      setPumpState(false, "Auto: Overflow sensor active (Tank Full)");
    }
    
    // 2. Check if Tank Level is Low
    else if (sensors.isLevelLow) {
      // if( Water tank has < 50% of water ) { ... }

      // Safety Check: Pump running for too long without flow?
      unsigned long elapsedTime = millis() - pumpStartTime;

      if (pumpIsRunning && elapsedTime >= SAFETY_TIMEOUT_MS && !sensors.isFlowActive) {
        // if(elpse time > 5 minutes and flow senosr is false)
        // { if(Pump is not off) { trun off the pump } }
        setPumpState(false, "Auto: SAFETY SHUTDOWN (No flow detected after 5 min)");
      
        saftyShutdwon = true;
      } 
      
      // Normal Fill Condition
      else {
        // else { if(Pump is not on) { trun on the pump } }
        setPumpState(true, "Auto: Water level low, initiating fill");
        
      }
    }
    
    // 3. Tank Level is Acceptable (> 50% and not full)
    else {
      // Tank is between 50% and 100%. Turn off the pump if it was running.
      setPumpState(false, "Auto: Water level acceptable (> 50%)");
     
    }
  }
}

/**
 * Utility function to switch mode (e.g., simulated Web UI input).
 */
void setControlMode(Mode newMode) {
    if (currentMode != newMode) {
        currentMode = newMode;
        Serial.print("Mode changed to: ");
        Serial.println(currentMode == AUTO_MODE ? "AUTOMATIC" : "MANUAL");
        if(currentMode == AUTO_MODE){
          object["AUTO_MODE"]="ON";
        }else{
           object["AUTO_MODE"]="OFF";
        }
        serializeJson(doc,jsonString);
        ws.textAll(jsonString);

    }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
    if(type == WS_EVT_CONNECT){
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    } else if(type == WS_EVT_DISCONNECT){
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    } else if(type == WS_EVT_ERROR){
       DeserializationError error = deserializeJson(doc_rx, (char*)data);
    
    }else if(type == WS_EVT_DATA){
     
      messageId = client->id();
        // Handle incoming WebSocket data
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        if(info->final && info->index == 0 && info->len == len){
            if(info->opcode == WS_TEXT){
                data[len] = 0;
                //Serial.printf("Received text: %s\n", (char*)data);
                //ws.text(client->id(), "Echo: " + String((char*)data)); // Echo back the received message

                 StaticJsonDocument<200> doc_rx;  // create a JSON container
                 
                 DynamicJsonDocument doc(1024);
                deserializeJson(doc,  (char*)data);
               
                JsonObject::iterator it = doc.as<JsonObject>().begin();

                String choice = (it->key().c_str());
                if(choice == "buttonState" && currentMode == MANUAL_MODE){        
                  if(String(it->value()) == "true"){                     
                    manualPumpOnRequest = true;
                  }else{                  
                    manualPumpOffRequest = true;                 
                  }
                }else if(choice == "manualState"){
                  if(String(it->value()) == "true"){
                     setControlMode(MANUAL_MODE);
                  }else{
                       setControlMode(AUTO_MODE);
                  }

                }

            }
        }
        
    }
}




// --- ARDUINO SETUP AND LOOP ---

void setup() {


  Serial.begin(9600);
  delay(100);
  Serial.println("\n--- ESP8266 Water Tank Manager Initialized ---");

  WiFi.begin(ssid, password);                         // start WiFi interface
  Serial.println("Establishing connection to WiFi with SSID: " + String(ssid));     // print SSID to the serial interface for debugging
 
  while (WiFi.status() != WL_CONNECTED) {             // wait until WiFi is connected
    delay(1000);
    Serial.print("\nConnecting....");
  }
  Serial.print("Connected to network with IP address: ");
  Serial.println(WiFi.localIP());  

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", webpage);
    });

    // server.on("/data", HTTP_POST, [](AsyncWebServerRequest *request){
    //     if (request->hasParam("value", true)) {
    //         String value = request->getParam("value", true)->value();
    //         request->send(200, "text/plain", "Received: " + value);
    //     } else {
    //         request->send(400, "text/plain", "Missing 'value' parameter");
    //     }
    // });

   ws.onEvent(onWsEvent);
   server.addHandler(&ws);
   server.begin();

  Serial.println(WiFi.macAddress());
  WiFi.setHostname(esp_hostname);
  Serial.println(WiFi.getHostname());

  if (MDNS.begin(esp_hostname)) {
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error starting mDNS");
  }

  // Pin Setup
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(OVERFLOW_SENSOR_PIN, INPUT_PULLUP);  // Use INPUT_PULLUP for float/dry contact sensors
  pinMode(LEVEL_LOW_SENSOR_PIN, INPUT_PULLUP); // Use INPUT_PULLUP
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);      // Use INPUT_PULLUP

  pinMode(SENSOR_TRIGER_PIN, OUTPUT);
  pinMode(SENSOR_ECHO_PIN, INPUT);


  // Initial pump state is OFF
  digitalWrite(PUMP_RELAY_PIN, LOW);
  pumpIsRunning = false;
  
  // Start in AUTO mode
  setControlMode(AUTO_MODE);
  
  // Set initial simulated sensor states for a test scenario:
  // Tank is low, no overflow, starting to fill, flow is initially active.
  Serial.println("Starting in a 'Tank Low' scenario...");
}

void loop(){

  MDNS.update(); // Important for mDNS to function

  toggle = !toggle;
  object["TOGGLE"]= toggle;
  serializeJson(doc,jsonString); 
  ws.textAll(jsonString);

  if(saftyShutdwon)
  {
    
    if(!saftyMessage){
        object["SAFTY_OFF"]= true;
        serializeJson(doc,jsonString); 
        ws.textAll(jsonString);
        saftyMessage = true;
    }
    delay(2000);
    return;
    
  }else{

  
      // Read the current physical sensor states
      SensorReadings currentSensors = readSensors();
      
      // Run the core control logic
      manageTank(currentSensors);
      
      // Simulation/Debugging Info (Optional)
      Serial.print("Mode: ");
      Serial.print(currentMode == AUTO_MODE ? "AUTO" : "MANUAL");
      Serial.print(" | Low: ");
      Serial.print(currentSensors.isLevelLow);
      Serial.print(" | Overflow: ");
      Serial.print(currentSensors.isOverflowActive);
      Serial.print(" | Flow: ");
      Serial.print(currentSensors.isFlowActive);
      Serial.print(" | Pump RUNNING: ");
      Serial.println(pumpIsRunning);

      if(String(currentSensors.isOverflowActive) == "1"){
        object["OVER_FLOW"]= "ON";
      }else if(String(currentSensors.isOverflowActive) == "0"){
        object["OVER_FLOW"]= "OFF";
      } 
      if(String(currentSensors.isFlowActive) == "1"){
        object["FLOW"]= "ON";
      }else if(String(currentSensors.isFlowActive) == "0"){
        object["FLOW"]= "OFF";
      }

      object["WATER_LEVEL"] = waterLevelReading;
      serializeJson(doc,jsonString);  
      
      ws.textAll(jsonString);

      delay(2000); // Check the state every 5 seconds
  }
  
}
