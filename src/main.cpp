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
bool invertMotor1 = false;  // Set to true to reverse motor 1
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

// LED mode
enum LedMode { OFF, STATIC, SPEED_BASED, RAINBOW, BREATHING };
LedMode currentLedMode = SPEED_BASED;
CRGB staticColor = CRGB::Blue;
int currentSpeed = 0;

// LED timing - non-blocking
unsigned long lastLedUpdate = 0;
const unsigned long LED_UPDATE_INTERVAL = 50;  // Update every 50ms (20Hz) instead of every loop

// Set motor speed and direction
// speed: -255 to 255 (negative = reverse, positive = forward)
void setMotor(int motorNum, int speed) {
  int dirPin = (motorNum == 1) ? DIR1 : DIR2;
  int pwmPin = (motorNum == 1) ? PWM1 : PWM2;
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
  ledcWrite(pwmPin, speed);
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
<h1> LEO Control</h1>
<div class="status">Ready to drive</div>

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
  <div class="ledTitle"> LEO LED Control</div>
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

  // Setup PWM channels for motor speed control (ESP32-S2 compatible)
  ledcAttach(PWM1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PWM2, PWM_FREQ, PWM_RESOLUTION);

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

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  // Only update LED at specified interval to reduce latency
  unsigned long currentMillis = millis();
  if (currentMillis - lastLedUpdate >= LED_UPDATE_INTERVAL) {
    lastLedUpdate = currentMillis;
    updateLED();
  }
}