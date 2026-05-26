#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>

const char* ssid = "ESP32-AUTO";
const char* password = "12345678";

WebServer server(80);

// Motor pins
int ENA = 9;  //not used
int DIR1 = 3;
int PWM1 = 5;

int ENB = 17;  //not used
int DIR2 = 1;
int PWM2 = 7;

// Motor direction inversion flags (change these if motors spin wrong way)
bool invertMotor1 = true;  // Set to true to reverse motor 1
bool invertMotor2 = false;  // Set to true to reverse motor 2

// PWM settings
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;
const int PWM_CHANNEL1 = 0;
const int PWM_CHANNEL2 = 1;

// LED settings
#define DATA_PIN 18
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

// Battery voltage measurement settings
#define BATTERY_PIN 10  // ADC pin for battery voltage divider
const float R_ABOVE = 220000.0;  // 220 kΩ
const float R_BELOW = 100000.0;  // 100 kΩ
float batteryVoltage = 0.0;

// LED mode
enum LedMode { OFF, STATIC, SPEED_BASED, RAINBOW, BREATHING };
LedMode currentLedMode = SPEED_BASED;
CRGB staticColor = CRGB::Blue;
int currentSpeed = 0;

// LED timing - non-blocking
unsigned long lastLedUpdate = 0;
const unsigned long LED_UPDATE_INTERVAL = 50;  // Update every 50ms (20Hz) instead of every loop

// Battery measurement timing
unsigned long lastBatteryUpdate = 0;
const unsigned long BATTERY_UPDATE_INTERVAL = 2000;  // Update every 2 seconds

// Set motor speed and direction
// speed: -255 to 255 (negative = reverse, positive = forward)
void setMotor(int motorNum, int speed) {
  int dirPin = (motorNum == 1) ? DIR1 : DIR2;
  int pwmChannel = (motorNum == 1) ? PWM_CHANNEL1 : PWM_CHANNEL2;
  bool invert = (motorNum == 1) ? invertMotor1 : invertMotor2;

  // Apply inversion if needed
  if (invert) {
    speed = -speed;
  }

  // Set direction
  if (speed >= 0) {
    digitalWrite(dirPin, HIGH);
  } else {
    digitalWrite(dirPin, LOW);
    speed = -speed;
  }

  // Constrain and set PWM
  speed = constrain(speed, 0, 255);
  ledcWrite(pwmChannel, speed);
}

// Joystick control: x and y range from -100 to 100
void setJoystick(int x, int y) {
  // Calculate motor speeds for differential steering
  // y controls forward/backward, x controls turning

  int leftSpeed = y + x;   // Left motor
  int rightSpeed = y - x;  // Right motor

  // Constrain to -100..100 range first
  leftSpeed = constrain(leftSpeed, -100, 100);
  rightSpeed = constrain(rightSpeed, -100, 100);

  // Scale from -100..100 to -255..255 for full PWM range
  leftSpeed = map(leftSpeed, -100, 100, -255, 255);
  rightSpeed = map(rightSpeed, -100, 100, -255, 255);

  setMotor(1, leftSpeed);
  setMotor(2, rightSpeed);

  // Track max speed for LED feedback (use the faster motor)
  currentSpeed = max(abs(leftSpeed), abs(rightSpeed));
}

void stopAuto() {
  setMotor(1, 0);
  setMotor(2, 0);
  currentSpeed = 0;
}

// Battery voltage measurement function
void readBatteryVoltage() {
  int adcValue = analogRead(BATTERY_PIN);
  // ESP32-S2 ADC: 0-4095 for 0-3.3V (12-bit)
  float vMeasured = (adcValue / 4095.0) * 3.3;
  // Calculate actual battery voltage using voltage divider formula
  batteryVoltage = vMeasured * ((R_ABOVE + R_BELOW) / R_BELOW);

  // Print to serial
  Serial.print("Battery: ");
  Serial.print(batteryVoltage, 2);
  Serial.print("V (ADC: ");
  Serial.print(adcValue);
  Serial.print(", Measured: ");
  Serial.print(vMeasured, 2);
  Serial.println("V)");
}

// LED control functions
void updateLED() {
  switch(currentLedMode) {
    case OFF:
      leds[0] = CRGB::Black;
      break;

    case STATIC:
      leds[0] = staticColor;
      break;

    case SPEED_BASED:
      if (currentSpeed < 85) {
        leds[0] = CRGB::Green;  // Slow - green (0-85)
      } else if (currentSpeed < 170) {
        leds[0] = CRGB::Orange;  // Medium - orange (85-170)
      } else {
        leds[0] = CRGB::Red;  // Fast - red (170-255)
      }
      break;

    case RAINBOW:
      {
        static uint8_t hue = 0;
        leds[0] = CHSV(hue++, 255, 255);
      }
      break;

    case BREATHING:
      {
        static uint8_t brightness = 0;
        static int8_t delta = 2;
        brightness += delta;
        if (brightness >= 250 || brightness <= 5) delta = -delta;
        leds[0] = staticColor;
        leds[0].fadeLightBy(255 - brightness);
      }
      break;
  }
  FastLED.show();
}

String pagina() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<style>
* { margin:0; padding:0; box-sizing:border-box; }

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: white;
}

h1 {
  font-size: 2rem;
  margin-bottom: 10px;
  text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
}

.status {
  font-size: 1rem;
  margin-bottom: 30px;
  opacity: 0.9;
  display: flex;
  align-items: center;
  gap: 15px;
}

.battery-container {
  display: flex;
  align-items: center;
  gap: 8px;
}

.battery {
  width: 60px;
  height: 28px;
  border: 3px solid rgba(255,255,255,0.8);
  border-radius: 4px;
  position: relative;
  background: rgba(0,0,0,0.3);
  padding: 3px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.3);
}

.battery::after {
  content: '';
  position: absolute;
  right: -8px;
  top: 50%;
  transform: translateY(-50%);
  width: 5px;
  height: 14px;
  background: rgba(255,255,255,0.8);
  border-radius: 0 2px 2px 0;
}

.battery-level {
  height: 100%;
  background: linear-gradient(90deg, #4ade80, #22c55e);
  border-radius: 2px;
  transition: width 0.5s ease, background 0.5s ease;
  box-shadow: 0 0 10px rgba(74,222,128,0.5);
}

.battery-level.low {
  background: linear-gradient(90deg, #fbbf24, #f59e0b);
  box-shadow: 0 0 10px rgba(251,191,36,0.5);
}

.battery-level.critical {
  background: linear-gradient(90deg, #ef4444, #dc2626);
  box-shadow: 0 0 10px rgba(239,68,68,0.5);
  animation: pulse 1s infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.6; }
}

.battery-text {
  font-size: 0.9rem;
  font-weight: bold;
  text-shadow: 1px 1px 2px rgba(0,0,0,0.5);
}

#joystickContainer {
  background: rgba(255,255,255,0.1);
  backdrop-filter: blur(10px);
  border-radius: 20px;
  padding: 30px;
  box-shadow: 0 8px 32px rgba(0,0,0,0.3);
}

#joystickBase {
  width: 280px;
  height: 280px;
  background: rgba(255,255,255,0.2);
  border: 4px solid rgba(255,255,255,0.3);
  border-radius: 50%;
  position: relative;
  box-shadow: inset 0 0 20px rgba(0,0,0,0.2);
}

#joystick {
  width: 100px;
  height: 100px;
  background: linear-gradient(135deg, #ff6b6b, #ee5a6f);
  border: 4px solid white;
  border-radius: 50%;
  position: absolute;
  top: 90px;
  left: 90px;
  cursor: grab;
  box-shadow: 0 4px 15px rgba(0,0,0,0.4);
  transition: transform 0.1s;
}

#joystick:active {
  cursor: grabbing;
  transform: scale(1.1);
}

#info {
  margin-top: 20px;
  font-size: 0.9rem;
  text-align: center;
  opacity: 0.8;
}

.coords {
  margin-top: 15px;
  padding: 10px;
  background: rgba(0,0,0,0.2);
  border-radius: 10px;
  font-family: monospace;
  font-size: 0.85rem;
}

#ledControls {
  margin-top: 30px;
  background: rgba(255,255,255,0.1);
  backdrop-filter: blur(10px);
  border-radius: 20px;
  padding: 20px;
  box-shadow: 0 8px 32px rgba(0,0,0,0.3);
}

.ledTitle {
  font-size: 1.2rem;
  margin-bottom: 15px;
  font-weight: bold;
}

.ledButtons {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  justify-content: center;
  margin-bottom: 15px;
}

.ledBtn {
  padding: 12px 20px;
  border: 2px solid white;
  border-radius: 12px;
  background: rgba(255,255,255,0.2);
  color: white;
  font-size: 0.9rem;
  cursor: pointer;
  transition: all 0.2s;
  font-weight: bold;
}

.ledBtn:hover {
  transform: translateY(-2px);
  box-shadow: 0 5px 15px rgba(0,0,0,0.3);
}

.ledBtn.active {
  background: rgba(255,255,255,0.4);
  box-shadow: 0 0 20px rgba(255,255,255,0.5);
}

.colorPicker {
  display: flex;
  gap: 8px;
  justify-content: center;
  flex-wrap: wrap;
}

.colorBtn {
  width: 45px;
  height: 45px;
  border: 3px solid white;
  border-radius: 50%;
  cursor: pointer;
  transition: all 0.2s;
}

.colorBtn:hover {
  transform: scale(1.1);
  box-shadow: 0 0 15px rgba(255,255,255,0.7);
}
</style>
</head>

<body>
<h1> Jaarwerk mobiel</h1>
<div class="status">
  <div class="battery-container">
    <div class="battery">
      <div class="battery-level" id="batteryLevel" style="width: 0%"></div>
    </div>
    <span class="battery-text"><span id="batteryVoltage">--</span> V</span>
  </div>
</div>

<div id="joystickContainer">
  <div id="joystickBase">
    <div id="joystick"></div>
  </div>
  <div id="info">
    <div class="coords">
      <div>X: <span id="xVal">0</span> | Y: <span id="yVal">0</span></div>
      <div>L: <span id="leftVal">0</span> | R: <span id="rightVal">0</span></div>
    </div>
  </div>
</div>

<div id="ledControls">
  <div class="ledTitle"> LED Control</div>
  <div class="ledButtons">
    <button class="ledBtn" onclick="setLedMode('off')">OFF</button>
    <button class="ledBtn active" onclick="setLedMode('speed')">Speed Mode</button>
    <button class="ledBtn" onclick="setLedMode('rainbow')">Rainbow</button>
    <button class="ledBtn" onclick="setLedMode('breathing')">Breathing</button>
    <button class="ledBtn" onclick="setLedMode('static')">Static Color</button>
  </div>
  <div class="colorPicker">
    <div class="colorBtn" style="background:#FF0000" onclick="setColor(255,0,0)"></div>
    <div class="colorBtn" style="background:#00FF00" onclick="setColor(0,255,0)"></div>
    <div class="colorBtn" style="background:#0000FF" onclick="setColor(0,0,255)"></div>
    <div class="colorBtn" style="background:#FFFF00" onclick="setColor(255,255,0)"></div>
    <div class="colorBtn" style="background:#FF00FF" onclick="setColor(255,0,255)"></div>
    <div class="colorBtn" style="background:#00FFFF" onclick="setColor(0,255,255)"></div>
    <div class="colorBtn" style="background:#FFFFFF" onclick="setColor(255,255,255)"></div>
    <div class="colorBtn" style="background:#FF8800" onclick="setColor(255,136,0)"></div>
  </div>
</div>

<script>
let base = document.getElementById("joystickBase");
let joy = document.getElementById("joystick");
let xVal = document.getElementById("xVal");
let yVal = document.getElementById("yVal");
let leftVal = document.getElementById("leftVal");
let rightVal = document.getElementById("rightVal");

let centerX = 140;
let centerY = 140;
let maxRadius = 90;
let active = false;

function updateDisplay(x, y) {
  xVal.textContent = x;
  yVal.textContent = y;

  let left = Math.round(y + x);
  let right = Math.round(y - x);
  leftVal.textContent = left;
  rightVal.textContent = right;
}

function sendJoystick(x, y) {
  fetch("/joy?x=" + x + "&y=" + y);
  updateDisplay(x, y);
}

function updateStick(clientX, clientY) {
  let rect = base.getBoundingClientRect();
  let x = clientX - rect.left;
  let y = clientY - rect.top;

  let dx = x - centerX;
  let dy = y - centerY;

  let distance = Math.sqrt(dx*dx + dy*dy);
  if (distance > maxRadius) {
    let angle = Math.atan2(dy, dx);
    dx = Math.cos(angle) * maxRadius;
    dy = Math.sin(angle) * maxRadius;
    x = centerX + dx;
    y = centerY + dy;
  }

  joy.style.left = (x - 50) + "px";
  joy.style.top = (y - 50) + "px";

  let joyX = Math.round((dx / maxRadius) * 100);
  let joyY = Math.round((-dy / maxRadius) * 100);

  sendJoystick(joyX, joyY);
}

function resetStick() {
  active = false;
  joy.style.left = "90px";
  joy.style.top = "90px";
  sendJoystick(0, 0);
}

base.addEventListener("touchstart", e => {
  e.preventDefault();
  active = true;
});

base.addEventListener("touchend", e => {
  e.preventDefault();
  resetStick();
});

base.addEventListener("touchmove", e => {
  e.preventDefault();
  if (!active) return;
  updateStick(e.touches[0].clientX, e.touches[0].clientY);
});

base.addEventListener("mousedown", e => {
  active = true;
});

base.addEventListener("mouseup", e => {
  resetStick();
});

base.addEventListener("mouseleave", e => {
  if (active) resetStick();
});

base.addEventListener("mousemove", e => {
  if (!active) return;
  updateStick(e.clientX, e.clientY);
});

// LED control functions
function setLedMode(mode) {
  fetch("/led?mode=" + mode);
  document.querySelectorAll('.ledBtn').forEach(btn => btn.classList.remove('active'));
  event.target.classList.add('active');
}

function setColor(r, g, b) {
  fetch("/led?mode=static&r=" + r + "&g=" + g + "&b=" + b);
  document.querySelectorAll('.ledBtn').forEach(btn => btn.classList.remove('active'));
  document.querySelectorAll('.ledBtn')[4].classList.add('active');
}

// Update battery voltage periodically
function updateBattery() {
  fetch("/battery")
    .then(response => response.text())
    .then(voltage => {
      const voltageVal = parseFloat(voltage);
      document.getElementById('batteryVoltage').textContent = voltage;

      // Calculate battery percentage (4.2V = 100%, 3.4V = 0%)
      const minVoltage = 3.4;
      const maxVoltage = 4.2;
      let percentage = ((voltageVal - minVoltage) / (maxVoltage - minVoltage)) * 100;
      percentage = Math.max(0, Math.min(100, percentage)); // Clamp between 0-100

      // Update battery level width
      const batteryLevel = document.getElementById('batteryLevel');
      batteryLevel.style.width = percentage + '%';

      // Update battery color based on level
      batteryLevel.classList.remove('low', 'critical');
      if (percentage <= 20) {
        batteryLevel.classList.add('critical');
      } else if (percentage <= 40) {
        batteryLevel.classList.add('low');
      }
    });
}

// Update battery every 2 seconds
setInterval(updateBattery, 2000);
updateBattery();  // Initial update
</script>

</body>
</html>
)rawliteral";
}

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(DIR1, OUTPUT);
  pinMode(DIR2, OUTPUT);
  pinMode(ENB, OUTPUT);

  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);

  // Setup battery ADC pin
  pinMode(BATTERY_PIN, INPUT);

  // Setup PWM channels for motor speed control
  ledcSetup(PWM_CHANNEL1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PWM1, PWM_CHANNEL1);

  ledcSetup(PWM_CHANNEL2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PWM2, PWM_CHANNEL2);

  // Initialize LED
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(100);
  leds[0] = CRGB::Green;
  FastLED.show();

  WiFi.softAP(ssid, password);
  Serial.begin(115200);
  Serial.println("ESP32 Auto Remote Control");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() {
    server.send(200, "text/html", pagina());
  });

  server.on("/joy", []() {
    if (server.hasArg("x") && server.hasArg("y")) {
      int x = server.arg("x").toInt();
      int y = server.arg("y").toInt();
      setJoystick(x, y);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/led", []() {
    if (server.hasArg("mode")) {
      String mode = server.arg("mode");

      if (mode == "off") {
        currentLedMode = OFF;
      } else if (mode == "speed") {
        currentLedMode = SPEED_BASED;
      } else if (mode == "rainbow") {
        currentLedMode = RAINBOW;
      } else if (mode == "breathing") {
        currentLedMode = BREATHING;
      } else if (mode == "static") {
        currentLedMode = STATIC;
        if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
          staticColor = CRGB(server.arg("r").toInt(),
                            server.arg("g").toInt(),
                            server.arg("b").toInt());
        }
      }
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/battery", []() {
    char voltageStr[10];
    dtostrf(batteryVoltage, 4, 2, voltageStr);
    server.send(200, "text/plain", voltageStr);
  });

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  // Only update LED at specified interval to reduce latency
  if (currentMillis - lastLedUpdate >= LED_UPDATE_INTERVAL) {
    lastLedUpdate = currentMillis;
    updateLED();
  }

  // Update battery voltage every 2 seconds
  if (currentMillis - lastBatteryUpdate >= BATTERY_UPDATE_INTERVAL) {
    lastBatteryUpdate = currentMillis;
    readBatteryVoltage();
  }
}