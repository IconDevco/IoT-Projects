#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include "time.h"
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

AsyncWebServer server(80);
Preferences prefs;

const char* setup_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>MKlock Setup</title></head>
<body style="font-family: sans-serif; text-align: center;">
  <h2>Welcome to MKlock!</h2>
  <p id="status">Grabbing your current time…</p>
  <form id="setupForm">
    <p><input name="ssid" placeholder="Wi-Fi Network" required></p>
    <p><input name="password" placeholder="Wi-Fi Password" type="password" required></p>
    <input type="hidden" name="hour">
    <input type="hidden" name="minute">
    <p><button type="submit">Submit & Sync Clock</button></p>
  </form>

<script>
  const form = document.getElementById("setupForm");
  const hourField = form.querySelector("input[name='hour']");
  const minuteField = form.querySelector("input[name='minute']");
  const status = document.getElementById("status");

  const now = new Date();
  const hour = now.getHours();
  const minute = now.getMinutes();
  hourField.value = hour;
  minuteField.value = minute;
  status.innerText = `Detected time: ${hour.toString().padStart(2, '0')}:${minute.toString().padStart(2, '0')}`;

  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    const data = new FormData(form);
    const payload = Object.fromEntries(data.entries());

    const res = await fetch("/submit", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify(payload)
    });

    status.innerText = "Clock set! You may now reconnect to your usual Wi-Fi.";
    form.remove();
  });
</script>
</body>
</html>
)rawliteral";

#define LED_PIN     10
#define NUM_LEDS    36
#define BRIGHTNESS  125

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Simulated start time
int hour = 12;
int minute = 30;
int prev_minute_pos = -1;
int prev_hour_pos = -1;
unsigned long lastUpdate = 0;
const unsigned long interval = 60000; 

void setup() {

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("MKlock_Setup");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP); // Should be 192.168.4.1

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", setup_page);
  });

 prefs.begin("mklock", true);
String ssid = prefs.getString("ssid", "");
String password = prefs.getString("password", "");
hour = prefs.getInt("hour", 12);
minute = prefs.getInt("minute", 30);
prefs.end();

if (ssid != "") {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi");

  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiTimeout < 5000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected, syncing time.");
    configTime(-25200, 0, "pool.ntp.org"); // update with your UTC offset
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      hour = timeinfo.tm_hour;
      minute = timeinfo.tm_min;
    } else {
      Serial.println("NTP failed — using last saved or default time.");
    }
    return; // all good — exit setup()
  }
}

// Wi-Fi failed or credentials not set — start setup mode
WiFi.mode(WIFI_AP);
WiFi.softAP("MKlock_Setup");
Serial.println("Started access point for configuration.");
IPAddress apIP = WiFi.softAPIP();
Serial.println(apIP);
 

server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Parse incoming body as JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }

    int hour = doc["hour"] | 6;
    int minute = doc["minute"] | 30;
    String ssid = doc["ssid"] | "";
    String password = doc["password"] | "";

    Serial.printf("Received time: %02d:%02d, SSID: %s\n", hour, minute, ssid.c_str());

    // Save to preferences
    prefs.begin("mklock", false);
    prefs.clear();
    prefs.putInt("hour", hour);
    prefs.putInt("minute", minute);
    prefs.putString("ssid", ssid);
    prefs.putString("password", password);
    prefs.end();

    request->send(200, "text/plain", "Time & Wi-Fi saved");

    delay(1000);
    ESP.restart();
});
//server.begin(); uncomment this to fuck everything up
}




int rotate(int index) {
  // Adjust so LED 0 is at 6:00, LED 18 is 12:00
  return (index + 18) % NUM_LEDS;
}

void fadeHands(int fromHour, int toHour, int fromMin, int toMin) {
  const int fade_steps = 10;

  // Get starting and ending colors
  uint32_t fromHourColor = strip.getPixelColor(fromHour);
  uint32_t fromMinColor  = strip.getPixelColor(fromMin);

  for (int i = 0; i <= fade_steps; i++) {
    float t = i / float(fade_steps);

    // Interpolate colors
    int rHour = (1 - t) * ((fromHourColor >> 16) & 0xFF) + t * 255;
    int gHour = 0;
    int bHour = 0;

    int rMin = 0;
    int gMin = 0;
    int bMin = (1 - t) * ((fromMinColor) & 0xFF) + t * 255;

    // Don't clear — only set pixels of interest
    if (fromHour != toHour) strip.setPixelColor(fromHour, strip.Color((1 - t) * 255, 0, 0));
    strip.setPixelColor(toHour, strip.Color(rHour, gHour, bHour));

    if (fromMin != toMin) strip.setPixelColor(fromMin, strip.Color(0, 0, (1 - t) * 255));
    strip.setPixelColor(toMin, strip.Color(rMin, gMin, bMin));

    strip.show();
    delay(20);
  }

  // Hold final state
  strip.setPixelColor(toHour, strip.Color(255, 0, 0));
  strip.setPixelColor(toMin, strip.Color(0, 0, 255));
  strip.show();
}

void loop() {
  unsigned long now = millis();


  if (millis() - lastUpdate >= interval) {
    lastUpdate = millis();
    minute++;
    if (minute >= 60) {
      minute = 0;
      hour = (hour + 1) % 24;
    }
    Serial.printf("Time: %02d:%02d\n", hour, minute);
  }

    // Calculate LED positions
    int minute_led = int(minute * (36.0 / 60.0)); // 0–35
    int minute_pos = rotate(minute_led);

    float hour_raw = (hour % 12) * 3.0 + (minute / 20.0); // 3 LEDs per hour
    int hour_led = int(hour_raw) % NUM_LEDS;
    int hour_pos = rotate(hour_led);

    // First run? Skip fade
    if (prev_hour_pos == -1 || prev_minute_pos == -1) {
      strip.clear();
      strip.setPixelColor(hour_pos, strip.Color(255, 0, 0));
      strip.setPixelColor(minute_pos, strip.Color(0, 0, 255));
      strip.show();
    } else {
      fadeHands(prev_hour_pos, hour_pos, prev_minute_pos, minute_pos);
    }

    prev_hour_pos = hour_pos;
    prev_minute_pos = minute_pos;


  }
