#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// === ST7789V2 SPI & Backlight Pins ===
#define TFT_CS     4
#define TFT_RST    5
#define TFT_DC     7
#define TFT_SCLK   6
#define TFT_MOSI  10
#define TFT_BL     3

// === MPU6050 I²C Pins ===
#define I2C_SDA    8
#define I2C_SCL    9

// === Screen size ===
#define SCREEN_WIDTH   240
#define SCREEN_HEIGHT  280

// === Maze geometry ===
#define RINGS     8
#define SECTORS  16

// === Wall bit-masks ===
#define WALL_IN   0x01
#define WALL_OUT  0x02
#define WALL_CW   0x04
#define WALL_CCW  0x08

float last_ax = 0, last_ay = 0, last_az = 0;

// === Ball properties ===
const int BALL_RADIUS = 5;       // pixels
float ballR, ballA;              // polar coords: radius, angle
float ballVr = 0, ballVa = 0;    // radial & angular velocity

// globals
SPIClass         customSPI(FSPI);
Adafruit_ST7789  tft(&customSPI, TFT_CS, TFT_DC, TFT_RST);
Adafruit_MPU6050 mpu;

uint8_t cellWalls[RINGS][SECTORS];
bool    visited[RINGS][SECTORS];
int     entrySector;

float radiusStep, angleStep;

// ───────── PROTOTYPES ─────────
void generateMaze();
void carvePolar(int r,int s);
void drawPolarMaze();
void drawSmoothArc(int cx,int cy,int rad,float a0,float a1,uint16_t color);
void drawRadialLine(int cx,int cy,int r1,int r2,float a,uint16_t color);
void readTilt(float &pitch,float &roll);
void updateBall();
void drawBall();

const float TILT_GAIN = 0.02f;

// ───────── SETUP ─────────────
void setup() {
  Serial.begin(115200);
  delay(200);

  // backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // display init
  customSPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  tft.setRotation(0);           // portrait
  tft.fillScreen(ST77XX_BLACK);

  // MPU6050 init
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  // build maze
  randomSeed(micros());
  generateMaze();

  // compute geometry steps
  radiusStep = float(min(tft.width(), tft.height())) / (2.2 * RINGS);
  angleStep  = TWO_PI / SECTORS;

  // place ball at outer entry
  ballR = (RINGS - 1) * radiusStep + radiusStep * 0.5;
  ballA = entrySector * angleStep + angleStep * 0.5;

  // initial draw
  drawPolarMaze();
  drawBall();
}

int lastX = -1, lastY = -1;

void loop() {
  updateBall();

  // compute new pixel coords
  int cx = tft.width()/2, cy = tft.height()/2;
  int x = cx + int(ballR * cos(ballA));
  int y = cy + int(ballR * sin(ballA));

  // erase the old ball
  if (lastX >= 0) {
    tft.fillCircle(lastX, lastY, BALL_RADIUS, ST77XX_BLACK);
  }

  // draw the new ball
  tft.fillCircle(x, y, BALL_RADIUS, ST77XX_RED);

  lastX = x;
  lastY = y;

  delay(30);
}


// ───────── MAZE GENERATION ─────────
void generateMaze() {
  for (int r = 0; r < RINGS; r++) {
    for (int s = 0; s < SECTORS; s++) {
      visited[r][s]   = false;
      cellWalls[r][s] = WALL_IN | WALL_OUT | WALL_CW | WALL_CCW;
    }
  }
  carvePolar(0, 0);

  // open one outer wall as entry
  entrySector = random(SECTORS);
  cellWalls[RINGS-1][entrySector] &= ~WALL_OUT;
}

void carvePolar(int r,int s) {
  visited[r][s] = true;
  int dirs[4] = {0,1,2,3};
  // shuffle directions
  for (int i = 0; i < 4; i++) {
    int j = random(4);
    int t = dirs[i]; dirs[i] = dirs[j]; dirs[j] = t;
  }
  // carve
  for (int i = 0; i < 4; i++) {
    int d = dirs[i], nr = r, ns = s;
    uint8_t wHere = 0, wThere = 0;

    if (d == 0 && r < RINGS-1) { nr = r+1; wHere = WALL_OUT;  wThere = WALL_IN; }
    if (d == 1 && r > 0)        { nr = r-1; wHere = WALL_IN;   wThere = WALL_OUT;}
    if (d == 2)                 { ns = (s+1) % SECTORS; wHere = WALL_CW;  wThere = WALL_CCW; }
    if (d == 3)                 { ns = (s-1+SECTORS)%SECTORS; wHere=WALL_CCW; wThere=WALL_CW; }

    if (!visited[nr][ns]) {
      cellWalls[r][s]     &= ~wHere;
      cellWalls[nr][ns]   &= ~wThere;
      carvePolar(nr, ns);
    }
  }
}

// ───────── DRAW MAZE ─────────
void drawPolarMaze() {
  tft.fillScreen(ST77XX_BLACK);
  int cx = tft.width() / 2;
  int cy = tft.height() / 2;

  for (int r = 0; r < RINGS; r++) {
    int inR  = r * radiusStep;
    int outR = (r+1) * radiusStep;
    for (int s = 0; s < SECTORS; s++) {
      float a1 = s * angleStep;
      float a2 = (s+1) * angleStep;
      uint8_t w = cellWalls[r][s];

      if (w & WALL_IN )  drawSmoothArc(cx, cy, inR,  a1, a2, ST77XX_WHITE);
      if (w & WALL_OUT)  drawSmoothArc(cx, cy, outR, a1, a2, ST77XX_WHITE);
      if (w & WALL_CW )  drawRadialLine(cx, cy, inR, outR, a2, ST77XX_WHITE);
      if (w & WALL_CCW)  drawRadialLine(cx, cy, inR, outR, a1, ST77XX_WHITE);
    }
  }
}

// smooth arc by connecting points
void drawSmoothArc(int cx,int cy,int rad,float a0,float a1,uint16_t color) {
  float span = fabs(a1 - a0);
  int segs = max(8, int(rad * span / 8.0f));
  float da = span / segs;
  float angle = a0;
  int x0 = cx + rad * cos(angle);
  int y0 = cy + rad * sin(angle);
  for (int i = 1; i <= segs; i++) {
    angle = a0 + da * i;
    int x1 = cx + rad * cos(angle);
    int y1 = cy + rad * sin(angle);
    tft.drawLine(x0, y0, x1, y1, color);
    x0 = x1; y0 = y1;
  }
}

// straight radial wall
void drawRadialLine(int cx,int cy,int r1,int r2,float a,uint16_t color) {
  int x1 = cx + r1 * cos(a), y1 = cy + r1 * sin(a);
  int x2 = cx + r2 * cos(a), y2 = cy + r2 * sin(a);
  tft.drawLine(x1, y1, x2, y2, color);
}




// Call this in place of readTilt()
void updateGravityFromMPU() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  // raw accel in m/s²
  float gx = accel.acceleration.x;
  float gy = accel.acceleration.y;
  float gz = accel.acceleration.z;

  // project onto horizontal plane and normalize to 1g
  float mag = sqrt(gx*gx + gy*gy + gz*gz);
  if (mag > 0.01f) {
    gx = gx / mag;  // now unit vector
    gy = gy / mag;
    gz = gz / mag;
  }

  last_ax = gx;
  last_ay = gy;
  last_az = gz;
}


void updateBall() {
  updateGravityFromMPU();   // sets last_ax, last_ay

  // 1) gain
  const float TILT_GAIN = 0.02f;
  // 2) dead‑zone helper
  auto dz = [&](float v) {
    const float D = 0.05f;
    if (fabs(v) < D) return 0.0f;
    return (v > 0 ? v - D : v + D) / (1.0f - D);
  };
  float rawR = dz(last_ay), rawA = dz(last_ax);

  // 3) compute accel
  float aR =  rawR * TILT_GAIN;
  float aA = -rawA * TILT_GAIN;

  // 4) integrate with damping & clamp
  const float MAX_VR = 2.0f;
  const float MAX_VA = 0.05f;
  ballVr = constrain((ballVr + aR) * 0.98f, -MAX_VR, MAX_VR);
  ballVa = constrain((ballVa + aA) * 0.98f, -MAX_VA, MAX_VA);

  // 5) update polar pos
  float newR = ballR + ballVr;
  float newA = ballA + ballVa;
  if (newA < 0) newA += TWO_PI;
  else if (newA >= TWO_PI) newA -= TWO_PI;

  // 6) collision & bounds (your existing code)
  // … 

  ballR = newR;
  ballA = newA;
}




void drawBall() {
  int cx = tft.width()/2;
  int cy = tft.height()/2;
  int x = cx + int(ballR * cos(ballA));
  int y = cy + int(ballR * sin(ballA));
  tft.fillCircle(x, y, BALL_RADIUS, ST77XX_RED);
}