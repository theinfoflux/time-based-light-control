#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <EEPROM.h>

const char* ssid = "AndroidAP3DEC";
const char* password = "11122233";

WebServer server(80);

#define LIGHT_PIN 2

int onHour = 18, onMinute = 0;
int offHour = 23, offMinute = 0;

bool manualOverride = false;
bool lightState = false;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 5 * 3600;
const int daylightOffset_sec = 0;

// ---------------- WEB PAGE ----------------
String htmlPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Light Control</title>

<style>
body {
  margin: 0;
  font-family: 'Segoe UI', Arial, sans-serif;
  background: linear-gradient(135deg, #fff176, #FFD700);
  text-align: center;
}

h2 {
  margin-top: 20px;
  color: #263238;
  letter-spacing: 1px;
}

.card {
  background: linear-gradient(145deg, #0f2027, #203a43);
  margin: 20px auto;
  padding: 22px;
  border-radius: 22px;
  max-width: 360px;
  box-shadow: 0 0 25px rgba(0,0,0,0.4);
  color: white;
}

/* GLOWING CLOCK */
.clock {
  font-size: 46px;
  font-weight: bold;
  color: #ffffff;
  text-shadow:
    0 0 10px #ffffff,
    0 0 20px #ffeb3b,
    0 0 40px #ffc107;
  animation: glow 1.5s infinite alternate;
}

@keyframes glow {
  from { text-shadow: 0 0 10px #fff; }
  to   { text-shadow: 0 0 25px #ffeb3b, 0 0 50px #ffc107; }
}

/* STATUS TEXT */
.status {
  font-size: 22px;
  margin-bottom: 10px;
}

/* ON/OFF SIMPLE WHITE TEXT */
.on, .off {
  color: #ffffff;
  font-weight: bold;
  font-size: 24px;
  text-shadow: none;
}

/* SCHEDULE BOX */
.scheduleBox {
  background: linear-gradient(135deg, #FFD740, #FFC107); /* bright yellow-orange */
  color: #000000;  /* bold black text */
  padding: 12px;
  border-radius: 14px;
  font-weight: bold;
  margin-bottom: 12px;
  box-shadow: 0 0 15px rgba(255, 193, 7, 0.8);
}

/* BUTTONS */
button {
  width: 100%;
  padding: 14px;
  margin-top: 10px;
  border: none;
  border-radius: 30px;
  font-size: 16px;
  cursor: pointer;
  font-weight: bold;
}

/* ON/OFF MANUAL CONTROL BUTTONS */
.onbtn {
  background: linear-gradient(45deg, #00e676, #76ff03);
  color: #ffffff;
}

.offbtn {
  background: linear-gradient(45deg, #ff5252, #ff1744);
  color: #ffffff;
}

/* SAVE SCHEDULE BUTTON */
.savebtn {
  background: linear-gradient(45deg, #FFEB3B, #FFC107); /* bright yellow */
  color: #000000; /* bold black text */
  font-weight: bold;
}

/* INPUT FIELDS */
input {
  width: 100%;
  padding: 11px;
  border-radius: 12px;
  border: none;
  margin-top: 10px;
  font-size: 15px;
}

.footer {
  font-size: 12px;
  color: #37474f;
  margin-bottom: 15px;
}
</style>
</head>

<body>

<h2> Smart Light Control</h2>

<div class="card">
  <div>Current Time</div>
  <div class="clock" id="time">--:--:--</div>
</div>

<div class="card">
  <div class="status" id="status">Loading...</div>
  <button class="onbtn" onclick="control('on')">Turn ON</button>
  <button class="offbtn" onclick="control('off')">Turn OFF</button>
</div>

<div class="card">
  <div class="scheduleBox" id="schedule">
    Schedule: ON --:-- | OFF --:--
  </div>

  <form action="/set">
    <input type="time" name="on" required>
    <input type="time" name="off" required>
    <button class="savebtn" type="submit">Save Schedule</button>
  </form>
</div>

<div class="footer">
  ESP32-C3 | Time-Based Automation
</div>

<script>
function updateData(){
  fetch('/data').then(r=>r.json()).then(d=>{
    document.getElementById('time').innerHTML = d.time;
    document.getElementById('status').innerHTML =
      d.light ? "<span class='on'>LIGHT ON</span>" : "<span class='off'>LIGHT OFF</span>";
    document.getElementById('schedule').innerHTML =
      "Schedule: ON " + d.on + " | OFF " + d.off;
  });
}

function control(cmd){
  fetch('/control?state=' + cmd);
}

setInterval(updateData, 1000);
updateData();
</script>

</body>
</html>
)rawliteral";
}

// ---------------- BACKEND HANDLERS ----------------
void handleRoot() { server.send(200, "text/html", htmlPage()); }

void handleData() {
  struct tm timeinfo;
  char t[10] = "--:--:--";
  if (getLocalTime(&timeinfo)) strftime(t, sizeof(t), "%H:%M:%S", &timeinfo);

  char onT[6], offT[6];
  sprintf(onT, "%02d:%02d", onHour, onMinute);
  sprintf(offT, "%02d:%02d", offHour, offMinute);

  String json = "{";
  json += "\"time\":\"" + String(t) + "\",";
  json += "\"light\":" + String(lightState ? "true" : "false") + ",";
  json += "\"on\":\"" + String(onT) + "\",";
  json += "\"off\":\"" + String(offT) + "\"}";
  server.send(200, "application/json", json);
}

void handleControl() {
  manualOverride = true;
  lightState = (server.arg("state") == "on");
  digitalWrite(LIGHT_PIN, lightState);
  server.send(200, "text/plain", "OK");
}

void handleSet() {
  onHour = server.arg("on").substring(0,2).toInt();
  onMinute = server.arg("on").substring(3,5).toInt();
  offHour = server.arg("off").substring(0,2).toInt();
  offMinute = server.arg("off").substring(3,5).toInt();

  EEPROM.write(0, onHour);
  EEPROM.write(1, onMinute);
  EEPROM.write(2, offHour);
  EEPROM.write(3, offMinute);
  EEPROM.commit();

  manualOverride = false;
  server.sendHeader("Location","/");
  server.send(303);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  pinMode(LIGHT_PIN, OUTPUT);
  EEPROM.begin(32);

  onHour = EEPROM.read(0); onMinute = EEPROM.read(1);
  offHour = EEPROM.read(2); offMinute = EEPROM.read(3);

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(500);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/control", handleControl);
  server.on("/set", handleSet);
  server.begin();
}

// ---------------- LOOP ----------------
void loop() {
  server.handleClient();
  if(manualOverride) return;

  struct tm t;
  if(!getLocalTime(&t)) return;

  bool shouldOn =
    (t.tm_hour > onHour || (t.tm_hour == onHour && t.tm_min >= onMinute)) &&
    (t.tm_hour < offHour || (t.tm_hour == offHour && t.tm_min < offMinute));

  digitalWrite(LIGHT_PIN, shouldOn);
  lightState = shouldOn;
}
