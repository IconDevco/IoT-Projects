#include <Adafruit_NeoPixel.h>

#define LED_PIN     10
#define NUM_LEDS    36
#define BRIGHTNESS  150

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Simulated start time
int hour = 12;
int minute = 30;
int prev_minute_pos = -1;
int prev_hour_pos = -1;
unsigned long lastUpdate = 0;
const unsigned long interval = 600; // 0.6 sec per simulated minute

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();
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

  // Advance simulated time
  if (now - lastUpdate >= interval) {
    lastUpdate = now;

    minute++;
    if (minute >= 60) {
      minute = 0;
      hour = (hour + 1) % 24;
    }

    Serial.printf("Time is now %02d:%02d\n", hour, minute);

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
}
