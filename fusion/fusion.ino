#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// === Custom I2C Pins ===
#define I2C_SDA 8
#define I2C_SCL 9

// === Pin Definitions ===
#define TFT_CS     4
#define TFT_RST    5
#define TFT_DC     7
#define TFT_SCLK   6
#define TFT_MOSI   10
#define TFT_BL     3  // Backlight pin

// === Display Setup ===
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// === MPU6050 Setup ===
Adafruit_MPU6050 mpu;

// === Screen and Physics Settings ===
const int SCREEN_WIDTH = 240;
const int SCREEN_HEIGHT = 280;

// Gradient center point (can be moved by gyro)
float centerX = SCREEN_WIDTH / 2.0;
float centerY = SCREEN_HEIGHT / 2.0;

// Maximum radius for the gradient
const int maxRadius = min(SCREEN_WIDTH, SCREEN_HEIGHT) / 2;

// Gyro sensitivity and smoothing
float gyroSensitivity = 0.5;
float centerXVel = 0;
float centerYVel = 0;
float damping = 0.95; // Velocity damping factor

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-C3 Interactive Radial Gradient");
  
  // Initialize custom I2C pins
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Initialize backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // Initialize SPI and backlight
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Initialize display
  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_RED);
  
  
  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");
  
  // Configure MPU6050
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  Serial.println("Drawing initial gradient...");
  drawRadialGradient();
  Serial.println("Ready! Tilt the device to move the gradient center.");
}

void loop() {
  // Read gyroscope data
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  
  // Apply gyroscope input to center velocity
  centerXVel += g.gyro.y * gyroSensitivity; // Pitch affects X
  centerYVel += g.gyro.x * gyroSensitivity; // Roll affects Y
  
  // Apply damping to velocity
  centerXVel *= damping;
  centerYVel *= damping;
  
  // Update center position
  centerX += centerXVel;
  centerY += centerYVel;
  
  // Keep center within screen bounds with some padding
  int padding = 50;
  centerX = constrain(centerX, padding, SCREEN_WIDTH - padding);
  centerY = constrain(centerY, padding, SCREEN_HEIGHT - padding);
  
  // Redraw gradient with new center
  drawRadialGradient();
  
  // Small delay to prevent overwhelming the display
  delay(50);
}

void drawRadialGradient() {
  // Draw the gradient by calculating each pixel's distance from center
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      // Calculate distance from current center
      float dx = x - centerX;
      float dy = y - centerY;
      float distance = sqrt(dx * dx + dy * dy);
      
      // Normalize distance to 0-1 range
      float normalizedDistance = distance / maxRadius;
      if (normalizedDistance > 1.0) normalizedDistance = 1.0;
      
      // Create color based on distance
      uint16_t color = calculateGradientColor(normalizedDistance);
      
      // Draw the pixel
      tft.drawPixel(x, y, color);
    }
  }
}

uint16_t calculateGradientColor(float t) {
  // Create a dynamic gradient that shifts based on gyro
  // Base gradient: purple center to cyan edge
  uint8_t r, g, b;
  
  if (t < 0.3) {
    // Inner core: deep purple to magenta
    float localT = t / 0.3;
    r = (uint8_t)(128 + localT * 127);
    g = (uint8_t)(0 + localT * 100);
    b = (uint8_t)(128 + localT * 127);
  } else if (t < 0.7) {
    // Middle ring: magenta to blue
    float localT = (t - 0.3) / 0.4;
    r = (uint8_t)(255 - localT * 155);
    g = (uint8_t)(100 + localT * 55);
    b = (uint8_t)(255);
  } else {
    // Outer ring: blue to cyan
    float localT = (t - 0.7) / 0.3;
    r = (uint8_t)(100 - localT * 100);
    g = (uint8_t)(155 + localT * 100);
    b = (uint8_t)(255);
  }
  
  // Convert RGB to 565 format
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Alternative: Motion-reactive gradient
uint16_t calculateMotionGradient(float t) {
  // Create a gradient that changes color based on motion intensity
  float motionIntensity = sqrt(centerXVel * centerXVel + centerYVel * centerYVel);
  motionIntensity = constrain(motionIntensity, 0, 2.0) / 2.0; // Normalize
  
  uint8_t r, g, b;
  
  // Base color shifts with motion
  float hue = motionIntensity * 360.0 + t * 60.0; // Motion affects base hue
  float saturation = 1.0 - t * 0.3; // Decrease saturation towards edge
  float value = 1.0 - t * 0.5; // Decrease brightness towards edge
  
  hsvToRgb(hue, saturation, value, &r, &g, &b);
  
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void hsvToRgb(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b) {
  // Normalize hue to 0-360 range
  while (h >= 360.0) h -= 360.0;
  while (h < 0.0) h += 360.0;
  
  float c = v * s;
  float x = c * (1 - abs(fmod(h / 60.0, 2) - 1));
  float m = v - c;
  
  float rPrime, gPrime, bPrime;
  
  if (h >= 0 && h < 60) {
    rPrime = c; gPrime = x; bPrime = 0;
  } else if (h >= 60 && h < 120) {
    rPrime = x; gPrime = c; bPrime = 0;
  } else if (h >= 120 && h < 180) {
    rPrime = 0; gPrime = c; bPrime = x;
  } else if (h >= 180 && h < 240) {
    rPrime = 0; gPrime = x; bPrime = c;
  } else if (h >= 240 && h < 300) {
    rPrime = x; gPrime = 0; bPrime = c;
  } else {
    rPrime = c; gPrime = 0; bPrime = x;
  }
  
  *r = (uint8_t)((rPrime + m) * 255);
  *g = (uint8_t)((gPrime + m) * 255);
  *b = (uint8_t)((bPrime + m) * 255);
}