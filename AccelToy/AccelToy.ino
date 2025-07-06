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
const int CIRCLE_RADIUS = 120;
const int CIRCLE_CENTER_X = SCREEN_WIDTH / 2;
const int CIRCLE_CENTER_Y = SCREEN_HEIGHT / 2;

const int NUM_BALLS = 16;
const float GRAVITY = 0.98;
const float DAMPING = 0.95;
const float BOUNCE_DAMPING = 0.85;
const float SHAKE_THRESHOLD = 2.5;

enum BallState { ACTIVE, DESPAWNING, SPAWNING };

struct Ball {
  float x, y;
  float vx, vy;
  float gx, gy;
  uint16_t color;
  float size;
  float targetSize;
  BallState state;
  unsigned long stateStartTime;
};

Ball balls[NUM_BALLS];

// === Color Palette ===
const uint16_t colors[] = {
  ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE, ST77XX_YELLOW,
  ST77XX_MAGENTA, ST77XX_CYAN, ST77XX_WHITE, ST77XX_ORANGE
};

// === Double Buffering ===
uint16_t frameBuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

// === Previous Acceleration for Shake Detection ===
float last_ax = 0, last_ay = 0, last_az = 0;

void setup() {
  Serial.begin(115200);

  // Initialize I2C with custom pins
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
  Serial.println("MPU6050 ready");

  // Initialize SPI and display
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  // Initialize balls
  randomSeed(analogRead(0));
  for (int i = 0; i < NUM_BALLS; i++) {
    balls[i].x = CIRCLE_CENTER_X + random(-CIRCLE_RADIUS + 10, CIRCLE_RADIUS - 10);
    balls[i].y = CIRCLE_CENTER_Y + random(-CIRCLE_RADIUS + 10, CIRCLE_RADIUS - 10);
    balls[i].vx = random(-30, 30) / 10.0;
    balls[i].vy = random(-20, 5) / 10.0;
    balls[i].color = colors[i % 8];
    balls[i].size = random(4, 12);
    balls[i].gx = 0;
    balls[i].gy = GRAVITY;
    balls[i].state = ACTIVE;
balls[i].stateStartTime = millis();
  }
}

void loop() {
  updateGravityFromMPU();

  memset(frameBuffer, 0, sizeof(frameBuffer));
  drawBoundaryCircle();

  for (int i = 0; i < NUM_BALLS; i++) {
    updateBall(i);
    drawBallToBuffer(i);
  }

  tft.drawRGBBitmap(0, 0, frameBuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
  delay(12); // ~50 FPS
}

void updateGravityFromMPU() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float gx = a.acceleration.x;
  float gy = a.acceleration.y;
  float gz = a.acceleration.z;

  // Tilt-based gravity
  float mag = sqrt(gx * gx + gy * gy);
  if (mag > 0.01) {
    gx = (gx / mag) * GRAVITY;
    gy = (gy / mag) * GRAVITY;

    for (int i = 0; i < NUM_BALLS; i++) {
      balls[i].gx = gx;
      balls[i].gy = gy;
    }
  }

  // Shake detection
  float delta = abs(gx - last_ax) + abs(gy - last_ay) + abs(gz - last_az);
  if (delta > SHAKE_THRESHOLD) {
    Serial.println("Shake detected!");
    shakeBalls();
  }

  last_ax = gx;
  last_ay = gy;
  last_az = gz;
}

void shakeBalls() {
  for (int i = 0; i < NUM_BALLS; i++) {
    balls[i].vx += random(-50, 50) / 10.0;
    balls[i].vy += random(-50, 50) / 10.0;
  }
}

void updateBall(int i) {
  Ball &ball = balls[i];

  unsigned long now = millis();

  switch (ball.state) {
    case ACTIVE:{
      // Normal physics
      ball.vx += ball.gx;
      ball.vy += ball.gy;
      ball.vx *= DAMPING;
      ball.vy *= DAMPING;
      ball.x += ball.vx;
      ball.y += ball.vy;

      // Bounce off circular boundary
      float dx = ball.x - CIRCLE_CENTER_X;
      float dy = ball.y - CIRCLE_CENTER_Y;
      float dist = sqrt(dx * dx + dy * dy);
      if (dist + ball.size > CIRCLE_RADIUS) {
        float nx = dx / dist;
        float ny = dy / dist;
        float overlap = (dist + ball.size) - CIRCLE_RADIUS;
        ball.x -= nx * overlap;
        ball.y -= ny * overlap;
        float dot = ball.vx * nx + ball.vy * ny;
        ball.vx -= 2 * dot * nx;
        ball.vy -= 2 * dot * ny;
        ball.vx *= BOUNCE_DAMPING;
        ball.vy *= BOUNCE_DAMPING;
      }

      // Randomly trigger despawn
      if (random(10000) < 50) { // ~0.05% chance per frame
        ball.state = DESPAWNING;
        ball.stateStartTime = now;
        ball.targetSize = ball.size;
      }
      break;
  }

case DESPAWNING: {
  ball.size -= 0.5;
  if (ball.size <= 0) {
    // Teleport and start spawning
    ball.x = random(CIRCLE_CENTER_X - CIRCLE_RADIUS + 10, CIRCLE_CENTER_X + CIRCLE_RADIUS - 10);
    ball.y = random(CIRCLE_CENTER_Y - CIRCLE_RADIUS + 10, CIRCLE_CENTER_Y + CIRCLE_RADIUS - 10);
    ball.vx = random(-30, 30) / 10.0;
    ball.vy = random(-20, 5) / 10.0;
    ball.state = SPAWNING;
    ball.stateStartTime = now;
    ball.size = 0;
  }
  break;
}

    case SPAWNING:{
      ball.size += 0.5;
      if (ball.size >= ball.targetSize) {
        ball.size = ball.targetSize;
        ball.state = ACTIVE;
      }
      break;
  }
  }

  // Collision detection can still run if desired
  if (ball.state == ACTIVE) {
    for (int j = i + 1; j < NUM_BALLS; j++) {
      if (balls[j].state == ACTIVE) {
        checkCollision(i, j);
      }
    }
  }
}

void checkCollision(int i, int j) {
  Ball &b1 = balls[i];
  Ball &b2 = balls[j];

  float dx = b2.x - b1.x;
  float dy = b2.y - b1.y;
  float distance = sqrt(dx * dx + dy * dy);

  if (distance < (b1.size + b2.size)) {
    float nx = dx / distance;
    float ny = dy / distance;

    float overlap = (b1.size + b2.size) - distance;
    b1.x -= nx * overlap * 0.5;
    b1.y -= ny * overlap * 0.5;
    b2.x += nx * overlap * 0.5;
    b2.y += ny * overlap * 0.5;

    float dvx = b2.vx - b1.vx;
    float dvy = b2.vy - b1.vy;
    float dvn = dvx * nx + dvy * ny;

    if (dvn > 0) return;

    float impulse = 2 * dvn / 2;

    b1.vx += impulse * nx * BOUNCE_DAMPING;
    b1.vy += impulse * ny * BOUNCE_DAMPING;
    b2.vx -= impulse * nx * BOUNCE_DAMPING;
    b2.vy -= impulse * ny * BOUNCE_DAMPING;
  }
}

void drawBallToBuffer(int i) {
  Ball &ball = balls[i];

  for (int16_t y = -ball.size; y <= ball.size; y++) {
    for (int16_t x = -ball.size; x <= ball.size; x++) {
      int16_t px = ball.x + x;
      int16_t py = ball.y + y;
      if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
        if (x * x + y * y <= ball.size * ball.size) {
          frameBuffer[py * SCREEN_WIDTH + px] = ball.color;
        }
      }
    }
  }

  int16_t hx = ball.x - ball.size / 3;
  int16_t hy = ball.y - ball.size / 3;
  int16_t hr = ball.size / 3;
  for (int16_t y = -hr; y <= hr; y++) {
    for (int16_t x = -hr; x <= hr; x++) {
      int16_t px = hx + x;
      int16_t py = hy + y;
      if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
        if (x * x + y * y <= hr * hr) {
          frameBuffer[py * SCREEN_WIDTH + px] = ST77XX_WHITE;
        }
      }
    }
  }
}

void drawBoundaryCircle() {
  for (int angle = 0; angle < 360; angle++) {
    float rad = radians(angle);
    int x = CIRCLE_CENTER_X + cos(rad) * CIRCLE_RADIUS;
    int y = CIRCLE_CENTER_Y + sin(rad) * CIRCLE_RADIUS;
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
      frameBuffer[y * SCREEN_WIDTH + x] = ST77XX_WHITE;
    }
  }
}