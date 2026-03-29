#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// === Pin Definitions ===
#define TFT_CS     4
#define TFT_RST    5
#define TFT_DC     7
#define TFT_SCLK   6
#define TFT_MOSI   10
#define TFT_BL     3  // Backlight pin
#define I2C_SDA    8
#define I2C_SCL    9

// === Display Setup ===
const int SCREEN_WIDTH = 240;
const int SCREEN_HEIGHT = 280;
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// === MPU Setup ===
Adafruit_MPU6050 mpu;

// === UI Constants ===
const uint16_t BAR_WIDTH = 200;
const uint16_t BAR_HEIGHT = 20;
const uint16_t BAR_X = (SCREEN_WIDTH - BAR_WIDTH) / 2;
const uint16_t BAR_Y = SCREEN_HEIGHT / 2;
const uint16_t TEXT_Y = BAR_Y - 32;
const uint16_t GRAY = tft.color565(169, 169, 169);
const uint16_t TURQUOISE = tft.color565(64, 224, 208);
const uint16_t RED = tft.color565(255, 0, 0);
const uint8_t TEXT_SIZE = 2;

// === Progress Configuration ===
const int numSteps = 9;
const char* stepLabels[numSteps] = {
  "booting", "connecting", "downloading", "installing",
  "verifying", "configuring", "authenticating", "initializing", ""
};

const unsigned long stepDurations[numSteps] = {
  60000, 12000, 217128000, 117504000, 60960000,
  34560000, 82944000, 126204000, 51828000
};

struct SegmentInfo {
  int subsegmentCount;
  unsigned long subDurations[60];
  unsigned long cumulative[60];
};

SegmentInfo steps[numSteps];

int currentStep = 0;
unsigned long startTime = 0;
unsigned long previousDotTime = 0;
const unsigned long dotInterval = 1000;
int dotCount = 0;

void setup() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  tft.setRotation(1);
  tft.fillScreen(GRAY);

  Wire.begin(I2C_SDA, I2C_SCL);
  mpu.begin();
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);

  generateTimingSegments();
  resetProgress();
}

void loop() {
  if (checkForShake()) {
    showError();
    delay(2000);
    tft.fillScreen(GRAY);
    resetProgress();
    return;
  }

  unsigned long elapsed = millis() - startTime;
  float progress = getProgressFraction(currentStep, elapsed);
  int fillWidth = BAR_WIDTH * progress;
  updateProgress(fillWidth);

if (currentStep >= 2 && currentStep < numSteps) {
  drawPercentage(progress);
}


  // Blinking dots independent of progress
  if (millis() - previousDotTime >= dotInterval && currentStep < numSteps) {
    previousDotTime = millis();
    dotCount = (dotCount + 1) % 4;
    drawStatusWithDots(stepLabels[currentStep], dotCount);
  }

  // Advance to next step if complete
  if (progress >= 1.0) {
    delay(1000);
    currentStep++;
    if (currentStep < numSteps) {
      startTime = millis();
      drawStatusWithDots(stepLabels[currentStep], 0);
    } else {
      drawStatusWithDots("All steps done", 0);
    }
  }
  if (currentStep < 2) {
  int clearX = BAR_X + BAR_WIDTH - TEXT_SIZE * 6 * 7; // ~7 chars width
  int clearY = BAR_Y - 32;
  tft.fillRect(clearX, clearY, TEXT_SIZE * 6 * 7, TEXT_SIZE * 8, GRAY);
}
}

void resetProgress() {
  currentStep = 0;
  startTime = millis();
  drawStatusWithDots(stepLabels[currentStep], 0);
  tft.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, ST77XX_WHITE);
}

bool checkForShake() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float threshold = 40.0; // Adjust sensitivity as needed
  return (fabs(a.acceleration.x) > threshold ||
          fabs(a.acceleration.y) > threshold ||
          fabs(a.acceleration.z - 9.81) > threshold);
}

void showError() {
  tft.fillScreen(RED);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(4);
  tft.setCursor((SCREEN_WIDTH - 6 * 4 * 6) / 2, SCREEN_HEIGHT / 2 - 30);
  tft.print("ERROR");

  tft.setTextSize(2);
  tft.setCursor((SCREEN_WIDTH - 8 * 2 * 6) / 2, SCREEN_HEIGHT / 2 + 10);
  tft.print("Resetting...");
}

void generateTimingSegments() {
  for (int s = 0; s < numSteps; s++) {
    int count = random(4, 19);
    steps[s].subsegmentCount = count;
    float rawWeights[18], weightSum = 0;
    for (int i = 0; i < count; i++) {
      rawWeights[i] = random(10, 100);
      weightSum += rawWeights[i];
    }
    unsigned long cumulative = 0;
    for (int i = 0; i < count; i++) {
      steps[s].subDurations[i] = (rawWeights[i] / weightSum) * stepDurations[s];
      steps[s].cumulative[i] = cumulative + steps[s].subDurations[i];
      cumulative = steps[s].cumulative[i];
    }
  }
}

void drawPercentage(float progress) {

  


  static float lastValue = -1.0;
  static char lastStr[10] = "";

  char currentStr[10];
  snprintf(currentStr, sizeof(currentStr), "%.2f%%", progress * 100.0);

  if (strcmp(currentStr, lastStr) != 0) {
    strcpy(lastStr, currentStr);
    lastValue = progress;

    int textWidth = strlen(currentStr) * TEXT_SIZE * 6;
    int percentX = BAR_X + BAR_WIDTH - textWidth;
    int percentY = BAR_Y - 32;

    // Clear and redraw
    tft.fillRect(percentX, percentY, textWidth, TEXT_SIZE * 8, GRAY);
    tft.setCursor(percentX, percentY);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(TEXT_SIZE);
    tft.print(currentStr);
  } else {
    // Still print, but skip clear
    int textWidth = strlen(currentStr) * TEXT_SIZE * 6;
    int percentX = BAR_X + BAR_WIDTH - textWidth;
    int percentY = BAR_Y - 32;

    tft.setCursor(percentX, percentY);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(TEXT_SIZE);
    tft.print(currentStr);
  }
}

float getProgressFraction(uint8_t stepIndex, unsigned long elapsed) {
  SegmentInfo& step = steps[stepIndex];
  int seg = 0;
  while (seg < step.subsegmentCount && elapsed > step.cumulative[seg]) seg++;
  if (seg >= step.subsegmentCount) return 1.0;
  unsigned long segStart = (seg == 0) ? 0 : step.cumulative[seg - 1];
  unsigned long segDuration = step.subDurations[seg];
  float segProgress = float(elapsed - segStart) / segDuration;
  return (seg + segProgress) / step.subsegmentCount;
}

void updateProgress(int fillWidth) {
  static int lastWidth = -1;
  fillWidth = constrain(fillWidth, 0, BAR_WIDTH - 2);
  if (fillWidth == lastWidth) return;
  lastWidth = fillWidth;
  tft.fillRect(BAR_X + 1, BAR_Y + 1, BAR_WIDTH - 2, BAR_HEIGHT - 2, GRAY);
  tft.fillRect(BAR_X + 1, BAR_Y + 1, fillWidth, BAR_HEIGHT - 2, TURQUOISE);
}

void drawStatusWithDots(const char* baseText, int numDots) {
  // Draw main status label above the bar
  tft.fillRect(BAR_X, TEXT_Y, BAR_WIDTH, TEXT_SIZE * 8, GRAY);
  tft.setCursor(BAR_X, TEXT_Y);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(TEXT_SIZE);
  tft.print(baseText);

  // Prepare dot string
  char dotStr[10] = "";
  for (int i = 0; i < numDots; i++) {
    strcat(dotStr, ". ");
  }

  // Measure dot width
  int dotWidth = strlen(dotStr) * TEXT_SIZE * 6;
  int dotX = BAR_X + (BAR_WIDTH - dotWidth) / 2;
  int dotY = BAR_Y + BAR_HEIGHT + 6; // beneath progress bar

  // Clear and draw centered dots
  tft.fillRect(BAR_X, dotY, BAR_WIDTH, TEXT_SIZE * 8, GRAY);
  tft.setCursor(dotX, dotY);
  tft.print(dotStr);
}