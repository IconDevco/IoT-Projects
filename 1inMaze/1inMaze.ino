#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// === ST7789V2 SPI & Backlight Pins ===
#define TFT_CS     4
#define TFT_RST    5
#define TFT_DC     7
#define TFT_SCLK   6
#define TFT_MOSI  10
#define TFT_BL     3

// === Screen Dimensions ===
#define SCREEN_WIDTH   240
#define SCREEN_HEIGHT  280

// === Maze Geometry ===
#define RINGS    8
#define SECTORS 16

// === Wall Bit-Masks ===
#define WALL_IN   0x01
#define WALL_OUT  0x02
#define WALL_CW   0x04
#define WALL_CCW  0x08

// === Globals ===
SPIClass        customSPI(FSPI);
Adafruit_ST7789 tft(&customSPI, TFT_CS, TFT_DC, TFT_RST);

uint8_t cellWalls[RINGS][SECTORS];
bool    visited[RINGS][SECTORS];
int     entrySector;

// ──────────── PROTOTYPES ────────────
void  generateMaze();
void  carvePolar(int r,int s);
void  drawPolarMaze();
void  drawArc(int cx,int cy,int rad,float a0,float a1,uint16_t col);
void  drawRadial(int cx,int cy,int r1,int r2,float a,uint16_t col);

// ───────────── SETUP ──────────────
void setup() {
  Serial.begin(115200);
  delay(300);

  // backlight on
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // init display
  customSPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  tft.setRotation(0);           // portrait
  tft.fillScreen(ST77XX_BLACK);

  randomSeed(micros());
  generateMaze();
  drawPolarMaze();
}

void loop() {
  // static puzzle
}

// ──────── MAZE GENERATION ─────────
void generateMaze() {
  // all walls, no visited
  for(int r=0;r<RINGS;r++){
    for(int s=0;s<SECTORS;s++){
      visited[r][s]   = false;
      cellWalls[r][s] = WALL_IN|WALL_OUT|WALL_CW|WALL_CCW;
    }
  }
  carvePolar(0,0);

  // open one exit on outer ring
  entrySector = random(SECTORS);
  cellWalls[RINGS-1][entrySector] &= ~WALL_OUT;
}

void carvePolar(int r,int s) {
  visited[r][s] = true;
  int dirs[4] = {0,1,2,3};
  // shuffle
  for(int i=0;i<4;i++){
    int j = random(4);
    int t = dirs[i]; dirs[i]=dirs[j]; dirs[j]=t;
  }
  for(int i=0;i<4;i++){
    int d=dirs[i],nr=r,ns=s;
    uint8_t wHere=0,wThere=0;
    if(d==0 && r<RINGS-1){ nr=r+1; wHere=WALL_OUT;  wThere=WALL_IN; }
    if(d==1 && r>0){        nr=r-1; wHere=WALL_IN;   wThere=WALL_OUT;}
    if(d==2){ ns=(s+1)%SECTORS; wHere=WALL_CW;  wThere=WALL_CCW;}
    if(d==3){ ns=(s-1+SECTORS)%SECTORS; wHere=WALL_CCW; wThere=WALL_CW;}

    if(!visited[nr][ns]){
      cellWalls[r][s]   &= ~wHere;
      cellWalls[nr][ns] &= ~wThere;
      carvePolar(nr,ns);
    }
  }
}

// ─────────── DRAWING ─────────────
void drawPolarMaze(){
  tft.fillScreen(ST77XX_BLACK);
  int cx=SCREEN_WIDTH/2, cy=SCREEN_HEIGHT/2;
  float rStep = float(min(SCREEN_WIDTH,SCREEN_HEIGHT)) / (2.2*RINGS);
  float aStep = TWO_PI/SECTORS;

  for(int r=0;r<RINGS;r++){
    int inR  = r * rStep;
    int outR = (r+1) * rStep;
    for(int s=0;s<SECTORS;s++){
      float a1 = s * aStep, a2 = (s+1) * aStep;
      uint8_t w = cellWalls[r][s];

      if(w & WALL_IN )  drawArc(cx,cy,inR ,a1,a2,ST77XX_WHITE);
      if(w & WALL_OUT)  drawArc(cx,cy,outR,a1,a2,ST77XX_WHITE);
      if(w & WALL_CW )  drawRadial(cx,cy,inR,outR,a2,ST77XX_WHITE);
      if(w & WALL_CCW)  drawRadial(cx,cy,inR,outR,a1,ST77XX_WHITE);
    }
  }
}

void drawArc(int cx,int cy,int rad,float a0,float a1,uint16_t col){
  const int STEPS=8;
  float da=(a1-a0)/STEPS;
  for(int i=0;i<=STEPS;i++){
    float a=a0+i*da;
    int x=cx+rad*cos(a), y=cy+rad*sin(a);
    tft.drawPixel(x,y,col);
  }
}

void drawRadial(int cx,int cy,int r1,int r2,float a,uint16_t col){
  int x1=cx+r1*cos(a), y1=cy+r1*sin(a);
  int x2=cx+r2*cos(a), y2=cy+r2*sin(a);
  tft.drawLine(x1,y1,x2,y2,col);
}