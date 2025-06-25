#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include "time.h"
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <DNSServer.h>

AsyncWebServer server(80);
Preferences prefs;

const char* setup_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>MKlock Setup</title></head>
<body style="font-family: sans-serif; text-align: center;">

  <h2>MKlock is Syncing</h2>
  <p id="status">Grabbing your current time and syncing to the clock</p>

<script>
  window.addEventListener("DOMContentLoaded", async () => {
    const now = new Date();
    const hour = now.getHours();
    const minute = now.getMinutes();

    const payload = {
      hour,
      minute,
    };

    try {
      const res = await fetch("/time-sync", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(payload)
      });

      if (res.ok) {
        document.getElementById("status").innerText = 
          `Clock set to ${hour.toString().padStart(2, '0')}:${minute.toString().padStart(2, '0')} — you're good to go!`;
      } else {
        document.getElementById("status").innerText = "Failed to sync time.";
      }
    } catch (err) {
      document.getElementById("status").innerText = "Error sending time. Try reconnecting.";
    }
  });
</script>

</body>
</html>
)rawliteral";

#define LED_PIN     10
#define NUM_LEDS    36
#define BRIGHTNESS  125
#define POT_PIN    A0

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Simulated start time
int hour = 12;
int minute = 0;
int prev_minute_pos = -1;
int prev_hour_pos = -1;
unsigned long lastUpdate = 0;
const unsigned long interval = 60000; 

const byte DNS_PORT = 53;
DNSServer dnsServer;

bool timeIsSet = (hour >= 0 && hour < 24) && (minute >= 0 && minute < 60);

void setup() {

//prefs.begin("mklock", false);
//prefs.clear();
//prefs.end();


  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  Serial.begin(115200);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", setup_page);
  });

 prefs.begin("mklock", true);
hour = prefs.getInt("hour", -1);
minute = prefs.getInt("minute", -1);
prefs.end();


// Wi-Fi failed or credentials not set — start setup mode
WiFi.mode(WIFI_AP);
WiFi.softAP("MKlock_Setup");

Serial.println("Started access point for configuration.");
IPAddress apIP = WiFi.softAPIP();
Serial.println(apIP);

dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
Serial.println("DNS server started — all domains redirect to MKlock");


server.begin();

server.on("/time-sync", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (data == nullptr || len == 0) {
      Serial.println("No data received in POST.");
      request->send(400, "text/plain", "No data received");
      return;
    }
    // Parse incoming body as JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }

     hour = doc["hour"] | -1;
     minute = doc["minute"] | -1;



    // Save to preferences
    prefs.begin("mklock", false);
    prefs.clear();
    prefs.putInt("hour", hour);
    prefs.putInt("minute", minute);
    prefs.end();

    request->send(200, "text/plain", "Time & Wi-Fi saved");

    delay(1000);
    Serial.printf("Storing: %02d:%02d\n", hour, minute);
    ESP.restart();
});


// Setup mode: pulse LED 0 while idle (basic visual cue)

if (WiFi.getMode() == WIFI_AP) {
  for (int i = 0; i <= 255; i += 5) {
      strip.setPixelColor(0, 0, 0, i); // fade in blue
  strip.show();
  delay(10);
  }
  for (int i = 255; i >= 0; i -= 5){
      strip.setPixelColor(0, 0, 0, i); // fade out blue
  strip.show();
  delay(10);
  }
}

server.begin(); 

WiFi.softAPdisconnect(true);
WiFi.mode(WIFI_OFF);
}
int potBrightness = -1;

int rotate(int index) {
  // Adjust so LED 0 is at 6:00, LED 18 is 12:00
  return (index + 18) % NUM_LEDS;
}

void fadeHands(int fromHour, int toHour, int fromMin, int toMin) {
  const int fade_steps = 10;

  // Get starting and ending colors
  uint32_t fromHourColor = strip.getPixelColor(fromHour * potBrightness);
  uint32_t fromMinColor  = strip.getPixelColor(fromMin * potBrightness);

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
  
   // prefs.begin("mklock", true);
  //int hour = prefs.getInt("hour", -1);
  //prefs.end();

  int potValue = analogRead(POT_PIN);
potBrightness = map(potValue, 0, 1023, 0, 255);

dnsServer.processNextRequest();

if (hour < 0) {
animateSetupScroll();
    return;
  }

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

const int tailLength = 36;  // number of trailing pixels
const int trailDecay = 12; // brightness decrease per pixel

void animateSetupScroll() {
  static int pos = 0;

  strip.clear();

  for (int i = 0; i < tailLength; i++) {
    int pixelIndex = (pos - i + NUM_LEDS) % NUM_LEDS;
    int brightness = 80 - (i * trailDecay);
    if (brightness < 0) brightness = 0;
    strip.setPixelColor(pixelIndex, strip.Color(brightness, brightness, brightness));
  }

  strip.show();
  delay(50);
  pos = (pos + 1) % NUM_LEDS;
}
