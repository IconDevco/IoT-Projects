#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
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

// === Perspective and Road Settings ===
const int HORIZON_Y = 80;  // Y position of the horizon/focal point
const int ROAD_FOCAL_X = SCREEN_WIDTH / 2;  // X position of focal point
const int ROAD_BASE_WIDTH = 200;  // Width of road at bottom of screen
const int ROAD_FOCAL_WIDTH = 4;   // Width of road at focal point
const int COCKPIT_HEIGHT = 100;   // Height of cockpit area from bottom

// === Game Variables ===
float steerAngle = 0;  // Current steering angle (-1.0 to 1.0)
float carPosition = 0; // Car position on road (-1.0 to 1.0, 0 = center)
float roadCurve = 0;   // Current road curve offset
int gameSpeed = 3;
unsigned long lastObstacleSpawn = 0;
unsigned long obstacleSpawnDelay = 2000;
int score = 0;
bool gameOver = false;

// === Obstacle System ===
#define MAX_OBSTACLES 4
struct Obstacle {
  float roadPosition;    // Position on road (0.0 = center, -1.0 = left edge, 1.0 = right edge)
  float distance;        // Distance from player (0.0 = at player, 1.0 = at horizon)
  bool active;
  uint16_t color;
};
Obstacle obstacles[MAX_OBSTACLES];

// === Traffic Lines ===
#define MAX_TRAFFIC_LINES 20
struct TrafficLine {
  float distance;        // Distance from player (0.0 = at player, 1.0 = at horizon)
  bool active;
};
TrafficLine trafficLines[MAX_TRAFFIC_LINES];

// === MPU6050 Variables ===
sensors_event_t a, g, temp;

// === Colors ===
#define CUSTOM_DARK_GREY 0x3186  // Custom dark grey (RGB: 48, 48, 48)
#define ROAD_COLOR CUSTOM_DARK_GREY
#define GRASS_COLOR 0x2589  // Dark green
#define LINE_COLOR ST77XX_YELLOW
#define HORIZON_COLOR 0x5ACF  // Sky blue
#define COCKPIT_COLOR ST77XX_BLACK
#define STEERING_WHEEL_COLOR 0x4208  // Dark grey
#define OBSTACLE_COLORS {ST77XX_RED, ST77XX_BLUE, ST77XX_MAGENTA, ST77XX_ORANGE}

void setup() {
  Serial.begin(115200);
  
  // Initialize custom I2C pins
  Wire.begin(I2C_SDA, I2C_SCL);
  
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
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("MPU6050 Error!");
    while (1) {
      delay(10);
    }
  }
  
  // Configure MPU6050
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  // Calibration message
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.println("Calibrating...");
  tft.setCursor(20, 130);
  tft.println("Keep Level!");
  delay(2000);
  
  // Initialize game
  initializeGame();
  
  Serial.println("3D Racing Game Started!");
}

void loop() {
  if (!gameOver) {
    updateInput();
    updateGame();
    drawGame();
    delay(16); // ~60 FPS
  } else {
    drawGameOver();
    // Reset game if tilted significantly
    mpu.getEvent(&a, &g, &temp);
    if (abs(a.acceleration.y) > 8.0) {
      resetGame();
    }
  }
}

void initializeGame() {
  steerAngle = 0;
  carPosition = 0;
  roadCurve = 0;
  gameSpeed = 3;
  score = 0;
  gameOver = false;
  
  // Initialize obstacles
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    obstacles[i].active = false;
  }
  
  // Initialize traffic lines
  for (int i = 0; i < MAX_TRAFFIC_LINES; i++) {
    trafficLines[i].distance = (float)i / MAX_TRAFFIC_LINES;
    trafficLines[i].active = true;
  }
  
  lastObstacleSpawn = millis();
}

void updateInput() {
  mpu.getEvent(&a, &g, &temp);
  
  // Fix steering direction: negate Y acceleration so left tilt = left steer
  float rawSteer = -a.acceleration.y / 5.0; // Negative to fix direction
  steerAngle = constrain(rawSteer, -1.0, 1.0);
  
  // Update car position based on steering, with road boundaries
  carPosition += steerAngle * 0.03; // Adjust sensitivity for car movement
  carPosition = constrain(carPosition, -0.8, 0.8); // Keep car on road
  
  // Apply steering to road curve for perspective effect (REVERSED for correct perspective)
  roadCurve -= steerAngle * 0.01; // Negative to reverse perspective
  roadCurve = constrain(roadCurve, -0.5, 0.5); // Tighter constraint
}

void updateGame() {
  // Move traffic lines toward player
  for (int i = 0; i < MAX_TRAFFIC_LINES; i++) {
    if (trafficLines[i].active) {
      trafficLines[i].distance -= gameSpeed * 0.01;
      if (trafficLines[i].distance <= 0) {
        trafficLines[i].distance = 1.0;
      }
    }
  }
  
  // Spawn new obstacles
  if (millis() - lastObstacleSpawn > obstacleSpawnDelay) {
    spawnObstacle();
    lastObstacleSpawn = millis();
    
    // Increase difficulty over time
    if (obstacleSpawnDelay > 1000) {
      obstacleSpawnDelay -= 50;
    }
    if (gameSpeed < 6) {
      gameSpeed += 0.2;
    }
  }
  
  // Update obstacles
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      obstacles[i].distance -= gameSpeed * 0.015;
      
      // Remove obstacles that have passed the player
      if (obstacles[i].distance <= 0) {
        obstacles[i].active = false;
        score += 10;
      }
      
          // Check collision with more precise detection
      checkCollision(i);
    }
  }
}

void checkCollision(int obstacleIndex) {
  if (obstacles[obstacleIndex].distance < 0.15 && obstacles[obstacleIndex].distance > 0.0) {
    // Calculate screen positions for both car and obstacle
    float t = 1.0 - obstacles[obstacleIndex].distance;
    int obstacleY = HORIZON_Y + t * (SCREEN_HEIGHT - COCKPIT_HEIGHT - HORIZON_Y);
    
    // Calculate road width and positions
    float roadWidth = ROAD_FOCAL_WIDTH + t * (ROAD_BASE_WIDTH - ROAD_FOCAL_WIDTH);
    int roadCenterX = ROAD_FOCAL_X + (roadCurve * (25 + t * 50));
    
    // Obstacle position and size
    int obstacleX = roadCenterX + obstacles[obstacleIndex].roadPosition * roadWidth * 0.4;
    int obstacleSize = 2 + t * 20;
    
    // Car position and size (car is always at bottom of screen)
    int carY = SCREEN_HEIGHT - COCKPIT_HEIGHT - 10; // Car is just above cockpit
    int carX = roadCenterX + carPosition * (ROAD_BASE_WIDTH * 0.4);
    int carWidth = 12;
    int carHeight = 20;
    
    // Check if obstacle is at car's level (close enough vertically)
    if (obstacleY > carY - carHeight && obstacleY < carY + carHeight) {
      // Check horizontal overlap using proper rectangle collision
      bool collision = (carX - carWidth/2 < obstacleX + obstacleSize/2) &&
                      (carX + carWidth/2 > obstacleX - obstacleSize/2);
      
      if (collision) {
        gameOver = true;
      }
    }
  }
}

void spawnObstacle() {
  // Find inactive obstacle slot
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obstacles[i].active) {
      obstacles[i].active = true;
      obstacles[i].distance = 1.0; // Start at horizon
      obstacles[i].roadPosition = random(-70, 70) / 100.0; // Random position on road
      
      // Random color
      uint16_t colors[] = OBSTACLE_COLORS;
      obstacles[i].color = colors[random(0, 4)];
      break;
    }
  }
}

void drawGame() {
  // Clear screen with sky color
  tft.fillScreen(HORIZON_COLOR);
  
  // Draw landscape
  drawLandscape();
  
  // Draw road with perspective
  drawRoad();
  
  // Draw traffic lines
  drawTrafficLines();
  
  // Draw obstacles
  drawObstacles();
  
  // Draw player car
  drawPlayerCar();
  
  // Draw cockpit
  drawCockpit();
  
  // Draw UI
  drawUI();
}

void drawLandscape() {
  // Draw grass/ground
  tft.fillRect(0, HORIZON_Y, SCREEN_WIDTH, SCREEN_HEIGHT - HORIZON_Y - COCKPIT_HEIGHT, GRASS_COLOR);
}

void drawRoad() {
  // Draw road as trapezoid with perspective
  int roadTopY = HORIZON_Y;
  int roadBottomY = SCREEN_HEIGHT - COCKPIT_HEIGHT;
  
  // Calculate road edges with curve effect and car position
  int curveOffset = -roadCurve * 50; // Negative to reverse perspective
  int topLeftX = ROAD_FOCAL_X - ROAD_FOCAL_WIDTH/2 + curveOffset;
  int topRightX = ROAD_FOCAL_X + ROAD_FOCAL_WIDTH/2 + curveOffset;
  int bottomLeftX = ROAD_FOCAL_X - ROAD_BASE_WIDTH/2 + (curveOffset * 2);
  int bottomRightX = ROAD_FOCAL_X + ROAD_BASE_WIDTH/2 + (curveOffset * 2);
  
  // Draw road surface using lines to create trapezoid
  for (int y = roadTopY; y < roadBottomY; y++) {
    float t = (float)(y - roadTopY) / (roadBottomY - roadTopY);
    int leftX = topLeftX + t * (bottomLeftX - topLeftX);
    int rightX = topRightX + t * (bottomRightX - topRightX);
    
    if (leftX >= 0 && rightX < SCREEN_WIDTH && rightX > leftX) {
      tft.drawFastHLine(leftX, y, rightX - leftX, ROAD_COLOR);
    }
  }
}

void drawTrafficLines() {
  // Draw center line segments
  for (int i = 0; i < MAX_TRAFFIC_LINES; i++) {
    if (trafficLines[i].active && trafficLines[i].distance > 0.1) {
      // Calculate position based on distance
      float t = 1.0 - trafficLines[i].distance;
      int y = HORIZON_Y + t * (SCREEN_HEIGHT - COCKPIT_HEIGHT - HORIZON_Y);
      
      // Calculate road width at this distance
      float roadWidth = ROAD_FOCAL_WIDTH + t * (ROAD_BASE_WIDTH - ROAD_FOCAL_WIDTH);
      int centerX = ROAD_FOCAL_X + (-roadCurve * (25 + t * 50)); // Negative to reverse perspective
      
      // Draw line segment
      int lineLength = 3 + t * 15; // Lines get longer as they get closer
      if (y > HORIZON_Y && y < SCREEN_HEIGHT - COCKPIT_HEIGHT) {
        tft.fillRect(centerX - 1, y, 2, lineLength, LINE_COLOR);
      }
    }
  }
}

void drawObstacles() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active && obstacles[i].distance > 0.05) {
      // Calculate screen position based on distance
      float t = 1.0 - obstacles[i].distance;
      int y = HORIZON_Y + t * (SCREEN_HEIGHT - COCKPIT_HEIGHT - HORIZON_Y);
      
      // Calculate road width and position
      float roadWidth = ROAD_FOCAL_WIDTH + t * (ROAD_BASE_WIDTH - ROAD_FOCAL_WIDTH);
      int roadCenterX = ROAD_FOCAL_X + (-roadCurve * (25 + t * 50)); // Negative to reverse perspective
      int obstacleX = roadCenterX + obstacles[i].roadPosition * roadWidth * 0.4;
      
      // Calculate obstacle size based on distance
      int obstacleSize = 2 + t * 20;
      
      if (y > HORIZON_Y && y < SCREEN_HEIGHT - COCKPIT_HEIGHT && 
          obstacleX > 0 && obstacleX < SCREEN_WIDTH) {
        // Draw obstacle as rectangle with better collision bounds
        tft.fillRect(obstacleX - obstacleSize/2, y - obstacleSize/2, 
                     obstacleSize, obstacleSize, obstacles[i].color);
        tft.drawRect(obstacleX - obstacleSize/2, y - obstacleSize/2, 
                     obstacleSize, obstacleSize, ST77XX_BLACK);
                     
        // Draw collision debug info (optional - remove for final version)
        if (false) { // Set to true to see collision bounds
          if (obstacles[i].distance < 0.15) {
            tft.drawRect(obstacleX - obstacleSize/2 - 2, y - obstacleSize/2 - 2, 
                         obstacleSize + 4, obstacleSize + 4, ST77XX_RED);
          }
        }
      }
    }
  }
}

void drawPlayerCar() {
  // Calculate car position on screen
  int carY = SCREEN_HEIGHT - COCKPIT_HEIGHT - 10;
  int roadCenterX = ROAD_FOCAL_X + (-roadCurve * 100); // Negative to reverse perspective
  int carX = roadCenterX + carPosition * (ROAD_BASE_WIDTH * 0.4);
  
  int carWidth = 12;
  int carHeight = 20;
  
  // Draw car body
  tft.fillRect(carX - carWidth/2, carY - carHeight/2, carWidth, carHeight, ST77XX_BLUE);
  tft.drawRect(carX - carWidth/2, carY - carHeight/2, carWidth, carHeight, ST77XX_WHITE);
  
  // Draw car details
  // Front windshield
  tft.fillRect(carX - carWidth/2 + 2, carY - carHeight/2 + 2, carWidth - 4, 4, ST77XX_CYAN);
  // Rear windshield
  tft.fillRect(carX - carWidth/2 + 2, carY + carHeight/2 - 6, carWidth - 4, 4, ST77XX_CYAN);
  
  // Draw collision debug info (optional - remove for final version)
  if (false) { // Set to true to see collision bounds
    tft.drawRect(carX - carWidth/2 - 2, carY - carHeight/2 - 2, 
                 carWidth + 4, carHeight + 4, ST77XX_RED);
  }
}

void drawCockpit() {
  int cockpitY = SCREEN_HEIGHT - COCKPIT_HEIGHT;
  
  // Draw cockpit base
  tft.fillRect(0, cockpitY, SCREEN_WIDTH, COCKPIT_HEIGHT, COCKPIT_COLOR);
  
  // Draw dashboard
  tft.fillRect(0, cockpitY, SCREEN_WIDTH, 30, STEERING_WHEEL_COLOR);
  
  // Draw steering wheel
  int wheelCenterX = SCREEN_WIDTH / 2;
  int wheelCenterY = cockpitY + 60;
  int wheelRadius = 35;
  
  // Outer wheel
  tft.fillCircle(wheelCenterX, wheelCenterY, wheelRadius, STEERING_WHEEL_COLOR);
  tft.drawCircle(wheelCenterX, wheelCenterY, wheelRadius, ST77XX_WHITE);
  
  // Inner wheel (hollow)
  tft.fillCircle(wheelCenterX, wheelCenterY, wheelRadius - 8, COCKPIT_COLOR);
  tft.drawCircle(wheelCenterX, wheelCenterY, wheelRadius - 8, ST77XX_WHITE);
  
  // Steering wheel spokes
  tft.drawLine(wheelCenterX - 15, wheelCenterY, wheelCenterX + 15, wheelCenterY, ST77XX_WHITE);
  tft.drawLine(wheelCenterX, wheelCenterY - 15, wheelCenterX, wheelCenterY + 15, ST77XX_WHITE);
  
  // Rotate wheel based on steering
  int spokeOffset = steerAngle * 15; // Increased visual feedback
  tft.drawLine(wheelCenterX - 15 + spokeOffset, wheelCenterY, 
               wheelCenterX + 15 + spokeOffset, wheelCenterY, ST77XX_YELLOW);
}

void drawUI() {
  // Score
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Score: ");
  tft.print(score);
  
  // Speed
  tft.setCursor(10, 35);
  tft.print("Speed: ");
  tft.print(gameSpeed * 10);
  
  // Car position indicator (shows where car is on road)
  tft.setCursor(10, 60);
  tft.print("Pos: ");
  tft.print((int)(carPosition * 100));
  
  // Debug collision info (remove for final version)
  if (false) { // Set to true to see collision debug info
    tft.setCursor(150, 10);
    tft.print("Collisions:");
    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (obstacles[i].active && obstacles[i].distance < 0.2) {
        tft.setCursor(150, 30 + i * 15);
        tft.print("D:");
        tft.print((int)(obstacles[i].distance * 100));
      }
    }
  }
}

void drawGameOver() {
  tft.fillScreen(ST77XX_BLACK);
  
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(3);
  tft.setCursor(20, 100);
  tft.println("CRASH!");
  
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 140);
  tft.print("Score: ");
  tft.print(score);
  
  tft.setCursor(20, 170);
  tft.println("Tilt to restart");
  
  delay(100);
}

void resetGame() {
  delay(500);
  initializeGame();
  tft.fillScreen(ST77XX_BLACK);
}