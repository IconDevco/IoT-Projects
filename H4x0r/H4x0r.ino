#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// === Pin Definitions ===
#define TFT_CS     4
#define TFT_RST    5
#define TFT_DC     7
#define TFT_SCLK   6
#define TFT_MOSI   10
#define TFT_BL     3  // Backlight pin

// === Display Setup ===
// For rotation 1, width and height swap:
const int SCREEN_WIDTH = 240;  // Landscape width
const int SCREEN_HEIGHT = 280; // Landscape height

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// === Text Settings ===
const int TEXT_SIZE = 1;
const int CHAR_WIDTH = 6 * TEXT_SIZE;    // Font width in pixels
const int LINE_HEIGHT = 8 * TEXT_SIZE;   // Font height in pixels

const int VERTICAL_MARGIN = 1;  // To avoid clipping descenders

const int MAX_LINES = SCREEN_HEIGHT / LINE_HEIGHT;  // Integer division floors automatically

String lineBuffer[MAX_LINES];  // Buffer holding current lines

// Hacker style code lines:
const char* codeLines[] = {
  "void crypt(char* data, int len) {",
  "  int i = 0;",
  "  while(i < len) {",
  "    data[i] ^= XOR_KEY;",
  "    i = (i + 1) % len;",
  "    if(i == 0) break;",
  "  }",
  "}",
  "",
  "int operator(int x) {",
  "  return ((x << 3) ^ (x >> 2)) + 0x42;",
  "}",
  "",
  "int main() {",
    "  char *p = (char*)0x1337;",
  "  for (; *p != '\\0'; ++p) {",
  "    *p ^= 0x55;",
  "    printf(\"%c\", *p);",
  "  char buffer[128];",
  "  int idx = 0;",
  "  ",
  "  memset(buffer, 0xAA, sizeof(buffer));",
  "  ",
  "  while(1) {",
  "    buffer[idx] = (char)(operator(idx) & 0xFF);",
  "    idx = (idx + 7) % sizeof(buffer);",
  "    crypt(buffer, sizeof(buffer));",
  "    ",
  "    for(int i=0; i<sizeof(buffer); i++) {",
  "      buffer[i] ^= (char)(idx & 0xFF); }" ,
  "    ",
  "    if(buffer[0] == '\\0') break;",
  "    ",
  "    if((rand() % 13) == 7) {",
  "      printf(\"[+] cycle %d complete\\n\", idx); }",
  "  }",
  "  ",
  "  if(buffer[10] == (char)(XOR_KEY ^ 0x37)) {",
  "    printf(\"! Insert vector activated !\\n\");",
  "    *((volatile int*)0x0) = 0xBADF00D;}",

  "  ",
  "  for(int x=0; x<1000000; x++) {",
  "    idx = (idx * 3 + x) % 256;",
  "  }",
  "  ",
  "  return 0;",
  "}",
    "struct _0xdead {",
  "  int _x; char _y; float _z;",
  "} __attribute__((packed));",
  "int __xor(int a, int b) {",
  "  return a ^ b ^ 0x1337;",
  "}",
  "char *morph(char *x) {",
  "  for (int i=0; x[i]; i++) {",
  "    x[i] = ((x[i] << 1) & 0xFF) | ((x[i] >> 7) & 0x01);",
  "  }",
  "  return x;",
  "}",
  NULL
};

void drawLine(int index) {
  int y = index * LINE_HEIGHT + VERTICAL_MARGIN;
  // Clear full line area (including margin above)
  tft.fillRect(0, y - VERTICAL_MARGIN, SCREEN_WIDTH, LINE_HEIGHT, ST77XX_BLACK);
  tft.setCursor(0, y);
  tft.print(lineBuffer[index]);
}

void scrollBuffer() {
  for (int i = 0; i < MAX_LINES - 1; i++) {
    lineBuffer[i] = lineBuffer[i + 1];
    drawLine(i);
  }
  // Clear bottom line buffer & screen
  lineBuffer[MAX_LINES - 1] = "";
  int y = (MAX_LINES - 1) * LINE_HEIGHT + VERTICAL_MARGIN;
  tft.fillRect(0, y - VERTICAL_MARGIN, SCREEN_WIDTH, LINE_HEIGHT, ST77XX_BLACK);
}

void typeLine(const char* newLine) {
  String currentLine = "";
  int bottomIndex = MAX_LINES - 1;

  for (int i = 0; newLine[i] != '\0'; i++) {
    currentLine += newLine[i];
    lineBuffer[bottomIndex] = currentLine;
    drawLine(bottomIndex);
    delay(30);  // Typing speed - adjust as you like
  }
}

void setup() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Backlight on

  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  tft.setRotation(0); // Landscape orientation
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK); // Green text on black bg
  tft.setTextSize(TEXT_SIZE);

  for (int i = 0; i < MAX_LINES; i++) {
    lineBuffer[i] = "";
  }
}

void loop() {
  static int codeIndex = 0;

  if (codeLines[codeIndex] == NULL) {
    codeIndex = 0;
  }

  scrollBuffer();
  typeLine(codeLines[codeIndex++]);
}
