#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32-AUTO";
const char* password = "12345678";

WebServer server(80);

// Motor pins
int ENA = 9;  
int DIR1 = 3;
int PWM1 = 5;

int ENB = 17;
int DIR2 = 1;
int PWM2 = 7;

void vooruit() {
  digitalWrite(DIR1, HIGH);
  digitalWrite(PWM1, HIGH);
  digitalWrite(DIR2, HIGH);
  digitalWrite(PWM2, HIGH);
}

void achteruit() {
  digitalWrite(DIR1, LOW);
  digitalWrite(PWM1, HIGH);
  digitalWrite(DIR2, LOW);
  digitalWrite(PWM2, HIGH);
}

void links() {
  digitalWrite(DIR1, LOW);
  digitalWrite(PWM1, HIGH);
  digitalWrite(DIR2, HIGH);
  digitalWrite(PWM2, HIGH);
}

void rechts() {
  digitalWrite(DIR1, HIGH);
  digitalWrite(PWM1, HIGH);
  digitalWrite(DIR2, LOW);
  digitalWrite(PWM2, HIGH);
}

void stopAuto() {
  digitalWrite(DIR1, LOW);
  digitalWrite(PWM1, LOW);
  digitalWrite(DIR2, LOW);
  digitalWrite(PWM2, LOW);
}

String pagina() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<style>
body { text-align:center; font-family:Arial; }

#joystickBase {
  width:200px;
  height:200px;
  background:#ddd;
  border-radius:50%;
  margin:50px auto;
  position:relative;
}

#joystick {
  width:80px;
  height:80px;
  background:#444;
  border-radius:50%;
  position:absolute;
  top:60px;
  left:60px;
}
</style>
</head>

<body>
<h2>ESP32 Auto Joystick</h2>

<div id="joystickBase">
  <div id="joystick"></div>
</div>

<script>
let base = document.getElementById("joystickBase");
let joy = document.getElementById("joystick");

let centerX = 100;
let centerY = 100;

let active = false;

function send(cmd){
  fetch("/" + cmd);
}

function updateStick(x, y){
  joy.style.left = (x - 40) + "px";
  joy.style.top  = (y - 40) + "px";

  let dx = x - centerX;
  let dy = y - centerY;

  let deadzone = 20;

  if(Math.abs(dx) < deadzone && Math.abs(dy) < deadzone){
    send("stop");
    return;
  }

  if(Math.abs(dx) > Math.abs(dy)){
    if(dx > 0){
      send("rechts");
    } else {
      send("links");
    }
  } else {
    if(dy > 0){
      send("achteruit");
    } else {
      send("vooruit");
    }
  }
}

base.addEventListener("touchstart", e => { active = true; });
base.addEventListener("touchend", e => { active = false; joy.style.left="60px"; joy.style.top="60px"; send("stop"); });

base.addEventListener("touchmove", e => {
  if(!active) return;
  let rect = base.getBoundingClientRect();
  let x = e.touches[0].clientX - rect.left;
  let y = e.touches[0].clientY - rect.top;
  updateStick(x,y);
});

base.addEventListener("mousedown", e => { active = true; });

base.addEventListener("mouseup", e => {
  active = false;
  joy.style.left="60px";
  joy.style.top="60px";
  send("stop");
});

base.addEventListener("mousemove", e => {
  if(!active) return;
  let rect = base.getBoundingClientRect();
  let x = e.clientX - rect.left;
  let y = e.clientY - rect.top;
  updateStick(x,y);
});
</script>

</body>
</html>
)rawliteral";
}

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(DIR1, OUTPUT);
  pinMode(PWM1, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(DIR2, OUTPUT);
  pinMode(PWM2, OUTPUT);

  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);

  WiFi.softAP(ssid, password);
  Serial.begin(115200);
  Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send(200, "text/html", pagina()); });
  server.on("/vooruit", []() { vooruit(); server.send(200, "text/plain", "vooruit"); });
  server.on("/achteruit", []() { achteruit(); server.send(200, "text/plain", "achteruit"); });
  server.on("/links", []() { links(); server.send(200, "text/plain", "links"); });
  server.on("/rechts", []() { rechts(); server.send(200, "text/plain", "rechts"); });
  server.on("/stop", []() { stopAuto(); server.send(200, "text/plain", "stop"); });

  server.begin();
}

void loop() {
  server.handleClient();
}