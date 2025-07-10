#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <esp_heap_caps.h>
#include <math.h>

// === Pin Definitions ===
#define TFT_CS    4
#define TFT_RST   5
#define TFT_DC    7
#define TFT_SCLK  6
#define TFT_MOSI  10
#define TFT_BL    3  // Backlight

// === Globals ===
Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);


// === Display Parameters ===
#define SCREEN_W  tft.width()
#define SCREEN_H  tft.height()

static const int16_t CENTER_X = SCREEN_W / 2;
static const int16_t CENTER_Y = SCREEN_H / 2;

// === Terrain Parameters ===
#define TERRAIN_W 16
#define TERRAIN_D 16

// === Camera / Projection Params ===
const float FOV_Y    = M_PI / 3.0f;  // 60° field‐of‐view
const float ASPECT   = float(SCREEN_W) / SCREEN_H;
const float Z_NEAR   = 0.1f;
const float Z_FAR    = 100.0f;

// === Types ===
struct Vec3f { float x, y, z; };
struct Vec4f { float x, y, z, w; };
struct Mat4  { float m[4][4]; };
struct Vec2i { int16_t x, y; };
struct Triangle { Vec3f v0, v1, v2; uint16_t color; };


// Terrain mesh (2 tris per quad)
static Triangle terrain[(TERRAIN_W-1)*(TERRAIN_D-1)*2];

// Frame-buffer in PSRAM
static const size_t FB_PIXELS = SCREEN_W * SCREEN_H;
uint16_t *frameBuffer = nullptr;

// Free‐fly camera state
Vec3f cameraPos   = {  8.0f, 15.0f, -20.0f };
float cameraYaw   = 0.0f;   // radians around Y
float cameraPitch = 0.0f;   // radians up/down
const Vec3f cameraUp = { 0,1,0 };

// Precomputed projection matrix
Mat4 projMat;

// ----------------------------------------------------------------------------
// 2D Value noise [-1,1]
// ----------------------------------------------------------------------------
float noise2d(int x,int y) {
  int n = x + y*57;
  n = (n<<13) ^ n;
  uint32_t v = (n*(n*n*15731 + 789221) + 1376312589u) & 0x7fffffffu;
  return 1.0f - float(v)/1073741824.0f;
}

// ----------------------------------------------------------------------------
// Vec3 & Vec4 helpers
// ----------------------------------------------------------------------------
Vec3f normalize(const Vec3f &v) {
  float l = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
  return { v.x/l, v.y/l, v.z/l };
}

Vec3f cross(const Vec3f &a, const Vec3f &b) {
  return {
    a.y*b.z - a.z*b.y,
    a.z*b.x - a.x*b.z,
    a.x*b.y - a.y*b.x
  };
}

float dot(const Vec3f &a, const Vec3f &b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

// ----------------------------------------------------------------------------
// 4×4 Matrix operations
// ----------------------------------------------------------------------------
Mat4 matMul(const Mat4 &A, const Mat4 &B) {
  Mat4 R;
  for(int i=0;i<4;i++){
    for(int j=0;j<4;j++){
      float sum=0;
      for(int k=0;k<4;k++) sum += A.m[i][k]*B.m[k][j];
      R.m[i][j]=sum;
    }
  }
  return R;
}

Vec4f matMul(const Mat4 &M, const Vec4f &v) {
  return {
    M.m[0][0]*v.x + M.m[0][1]*v.y + M.m[0][2]*v.z + M.m[0][3]*v.w,
    M.m[1][0]*v.x + M.m[1][1]*v.y + M.m[1][2]*v.z + M.m[1][3]*v.w,
    M.m[2][0]*v.x + M.m[2][1]*v.y + M.m[2][2]*v.z + M.m[2][3]*v.w,
    M.m[3][0]*v.x + M.m[3][1]*v.y + M.m[3][2]*v.z + M.m[3][3]*v.w
  };
}

// ----------------------------------------------------------------------------
// Build perspective projection matrix
// ----------------------------------------------------------------------------
Mat4 makePerspective(float fovY, float aspect, float zNear, float zFar) {
  float f = 1.0f / tanf(fovY * 0.5f);
  Mat4 P = { 0 };
  P.m[0][0] = f / aspect;
  P.m[1][1] = f;
  P.m[2][2] = (zFar + zNear) / (zNear - zFar);
  P.m[2][3] = (2 * zFar * zNear) / (zNear - zFar);
  P.m[3][2] = -1.0f;
  return P;
}

// ----------------------------------------------------------------------------
// Build view ("lookAt") matrix
// ----------------------------------------------------------------------------
Mat4 makeLookAt(const Vec3f &eye, const Vec3f &center, const Vec3f &up) {
  Vec3f fwd = normalize({ center.x-eye.x, center.y-eye.y, center.z-eye.z });
  Vec3f right = normalize(cross(fwd, up));
  Vec3f u   = cross(right, fwd);

  Mat4 M = { 0 };
  M.m[0][0]= right.x; M.m[0][1]= right.y; M.m[0][2]= right.z; M.m[0][3]= -dot(right, eye);
  M.m[1][0]=   u.x;    M.m[1][1]=   u.y;    M.m[1][2]=   u.z;    M.m[1][3]= -dot(u, eye);
  M.m[2][0]=-fwd.x;    M.m[2][1]=-fwd.y;    M.m[2][2]=-fwd.z;    M.m[2][3]=  dot(fwd, eye);
  M.m[3][3]=1.0f;
  return M;
}

// ----------------------------------------------------------------------------
// Shade a 5-6-5 color by brightness [0..1]
// ----------------------------------------------------------------------------
uint16_t shadeColor(uint16_t c, float b) {
  uint8_t r=(c>>11)&0x1F, g=(c>>5)&0x3F, bb=c&0x1F;
  r=uint8_t(r*b); g=uint8_t(g*b); bb=uint8_t(bb*b);
  return (r<<11)|(g<<5)|bb;
}

// ----------------------------------------------------------------------------
// Frame Buffer Helpers
// ----------------------------------------------------------------------------
inline void clearFB(uint16_t c) {
  for(size_t i=0;i<FB_PIXELS;i++) frameBuffer[i]=c;
}

inline void drawHLineFB(int x0,int x1,int y,uint16_t c){
  if(y<0||y>=SCREEN_H) return;
  if(x0>x1) {int t=x0;x0=x1;x1=t;}
  if(x1<0||x0>=SCREEN_W) return;
  if(x0<0)x0=0; if(x1>=SCREEN_W)x1=SCREEN_W-1;
  uint16_t *row = frameBuffer + y*SCREEN_W;
  for(int x=x0;x<=x1;x++) row[x]=c;
}

// ----------------------------------------------------------------------------
// Scanline triangle fill into FB
// ----------------------------------------------------------------------------
void fillFlatTop(const Vec2i &v0,const Vec2i &v1,const Vec2i &v2,uint16_t col){
  float inv1=float(v1.x-v0.x)/float(v1.y-v0.y),
        inv2=float(v2.x-v0.x)/float(v2.y-v0.y);
  float cur1=v0.x, cur2=v0.x;
  for(int y=v0.y;y<=v1.y;y++){
    drawHLineFB(int(cur1),int(cur2),y,col);
    cur1+=inv1; cur2+=inv2;
  }
}

void fillFlatBottom(const Vec2i &v0,const Vec2i &v1,const Vec2i &v2,uint16_t col){
  float inv1=float(v2.x-v0.x)/float(v2.y-v0.y),
        inv2=float(v2.x-v1.x)/float(v2.y-v1.y);
  float cur1=v0.x, cur2=v1.x;
  for(int y=v0.y;y<=v2.y;y++){
    drawHLineFB(int(cur1),int(cur2),y,col);
    cur1+=inv1; cur2+=inv2;
  }
}

void fillTriFB(Vec2i a,Vec2i b,Vec2i c,uint16_t col){
  if(a.y>b.y) {auto t=a;a=b;b=t;}
  if(a.y>c.y) {auto t=a;a=c;c=t;}
  if(b.y>c.y) {auto t=b;b=c;c=t;}
  if(b.y==c.y){
    fillFlatBottom(a,b,c,col);
  } else if(a.y==b.y){
    fillFlatTop(a,b,c,col);
  } else {
    float t=float(b.y-a.y)/float(c.y-a.y);
    Vec2i vi = { int(a.x + t*(c.x-a.x)), b.y };
    fillFlatTop(a,b,vi,col);
    fillFlatBottom(b,vi,c,col);
  }
}

// ----------------------------------------------------------------------------
// Render one triangle with MVP → FB
// ----------------------------------------------------------------------------
void renderTriFB(const Triangle &tri, const Mat4 &MVP) {
  // Transform vertices
  Vec4f hv0 = { tri.v0.x, tri.v0.y, tri.v0.z, 1.0f };
  Vec4f hv1 = { tri.v1.x, tri.v1.y, tri.v1.z, 1.0f };
  Vec4f hv2 = { tri.v2.x, tri.v2.y, tri.v2.z, 1.0f };

  Vec4f tv0 = matMul(MVP, hv0);
  Vec4f tv1 = matMul(MVP, hv1);
  Vec4f tv2 = matMul(MVP, hv2);

  // Clipping: skip if behind camera
  if(tv0.w <= 0 || tv1.w <= 0 || tv2.w <= 0) return;

  // Perspective divide & NDC → screen
  Vec2i p0 = {
    int16_t(tv0.x/tv0.w * CENTER_X + CENTER_X),
    int16_t(tv0.y/tv0.w * CENTER_Y + CENTER_Y)
  };
  Vec2i p1 = {
    int16_t(tv1.x/tv1.w * CENTER_X + CENTER_X),
    int16_t(tv1.y/tv1.w * CENTER_Y + CENTER_Y)
  };
  Vec2i p2 = {
    int16_t(tv2.x/tv2.w * CENTER_X + CENTER_X),
    int16_t(tv2.y/tv2.w * CENTER_Y + CENTER_Y)
  };

  // Lighting: flat per-triangle
  Vec3f e1 = { tri.v1.x-tri.v0.x, tri.v1.y-tri.v0.y, tri.v1.z-tri.v0.z };
  Vec3f e2 = { tri.v2.x-tri.v0.x, tri.v2.y-tri.v0.y, tri.v2.z-tri.v0.z };
  Vec3f N  = normalize(cross(e1,e2));
  float diff = dot(N, normalize({1,1,-1})); // example light
  if(diff<0) diff=0;
  float bright = 0.2f + 0.8f*diff;
  if(bright>1) bright=1;
  uint16_t litCol = shadeColor(tri.color, bright);

  // Rasterize
  fillTriFB(p0,p1,p2, litCol);
}

// ----------------------------------------------------------------------------
// Render scene with MVP
// ----------------------------------------------------------------------------
void renderSceneFB(const Mat4 &MVP) {
  size_t triCount = sizeof(terrain)/sizeof(terrain[0]);
  for(size_t i=0;i<triCount;i++){
    renderTriFB(terrain[i], MVP);
  }
}

// ----------------------------------------------------------------------------
// Build terrain mesh once
// ----------------------------------------------------------------------------
void generateTerrain(float scale) {
  int idx = 0;
  for(int z=0; z<TERRAIN_D-1; z++){
    for(int x=0; x<TERRAIN_W-1; x++){
      float h00 = noise2d(x,  z  ) * scale;
      float h10 = noise2d(x+1,z  ) * scale;
      float h01 = noise2d(x,  z+1) * scale;
      float h11 = noise2d(x+1,z+1) * scale;
      Vec3f a={float(x),   h00, float(z)};
      Vec3f b={float(x+1), h10, float(z)};
      Vec3f c={float(x),   h01, float(z+1)};
      Vec3f d={float(x+1), h11, float(z+1)};
      uint16_t baseCol = tft.color565(34,139,34);
      terrain[idx++] = {a,b,c,baseCol};
      terrain[idx++] = {b,d,c,baseCol};
    }
  }
}

// ----------------------------------------------------------------------------
// Setup & Loop
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  pinMode(TFT_BL, OUTPUT); 
  digitalWrite(TFT_BL, HIGH);

  tft.init(SCREEN_W, SCREEN_H);
  tft.setRotation(0);


 // Try PSRAM first:
  if(psramFound()){
    frameBuffer = (uint16_t*)heap_caps_malloc(
      FB_PIXELS * sizeof(uint16_t),
      MALLOC_CAP_SPIRAM
    );
    Serial.println("Using PSRAM");
  }
  // Fallback to any 8-bit–addressable RAM
  if(!frameBuffer){
    frameBuffer = (uint16_t*)heap_caps_malloc(
      FB_PIXELS * sizeof(uint16_t),
      MALLOC_CAP_8BIT
    );
    Serial.println("PSRAM not available, using internal RAM");
  }
  if(!frameBuffer){
    Serial.println("Allocation FAILED! Cannot continue.");
    while(true);
  }

Serial.print("FB ptr: 0x");  
Serial.print(uintptr_t(frameBuffer), HEX);  
Serial.print("  bytes: ");  
Serial.println(FB_PIXELS * sizeof(uint16_t));


  // build terrain
  generateTerrain(5.0f);

  // precompute projection matrix
  projMat = makePerspective(FOV_Y, ASPECT, Z_NEAR, Z_FAR);
}

void loop() {
  // update camera orientation
  cameraYaw   += 0.01f;
  cameraPitch += 0.002f;  // subtle tilt
  if(cameraYaw > 2*M_PI)   cameraYaw   -= 2*M_PI;
  if(cameraPitch >  M_PI/2) cameraPitch  =  M_PI/2;
  if(cameraPitch < -M_PI/2) cameraPitch  = -M_PI/2;

  // compute camera front vector
  Vec3f front = {
    cosf(cameraYaw)*cosf(cameraPitch),
    sinf(cameraPitch),
    sinf(cameraYaw)*cosf(cameraPitch)
  };
  Vec3f target = { cameraPos.x+front.x,
                   cameraPos.y+front.y,
                   cameraPos.z+front.z };

  // build view matrix
  Mat4 viewMat = makeLookAt(cameraPos, target, cameraUp);
  // MVP = P * V (model is identity)
  Mat4 MVP = matMul(projMat, viewMat);

  // render to frameBuffer
  clearFB(ST77XX_CYAN);
  renderSceneFB(MVP);


  // push full buffer
  tft.startWrite();
    tft.setAddrWindow(0,0,SCREEN_W,SCREEN_H);
    for(size_t i=0;i<FB_PIXELS;i++){
      tft.pushColor(frameBuffer[i]);
    }
  tft.endWrite();

  delay(200);  // ~30 FPS
}