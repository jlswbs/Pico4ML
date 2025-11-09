// IMU wireframe cube //

#include "hardware/structs/rosc.h"
#include "st7735_lcd.pio.h"
#include "ICM20948.h"

#define SERIAL_CLK_DIV 1.f
PIO pio = pio0;
uint sm = 0;
uint offset = pio_add_program(pio, &st7735_lcd_program);

#define PIN_DIN   11
#define PIN_CLK   10
#define PIN_CS    13
#define PIN_DC    9
#define PIN_RESET 7

#define WIDTH   80
#define HEIGHT  160

IMU_EN_SENSOR_TYPE enMotionSensorType;

static const uint8_t st7735_init_seq[] = {
  1,20,0x01, 1,10,0x11, 2,2,0x3A,0x05, 2,0,0x36,0x08,
  5,0,0x2A,0x00,0x18,0x00,0x67, 5,0,0x2B,0x00,0x00,0x00,0x9F,
  1,2,0x20, 1,2,0x13, 1,2,0x29, 0
};

static inline void seed_random_from_rosc(){
  uint32_t random=0,bit;
  volatile uint32_t *rnd_reg=(uint32_t*)(ROSC_BASE+ROSC_RANDOMBIT_OFFSET);
  for(int k=0;k<32;k++){
    while(1){
      bit = (*rnd_reg)&1;
      if(bit != ((*rnd_reg)&1)) break;
    }
    random = (random<<1)|bit;
  }
  srand(random);
  randomSeed(random);
}

static inline void lcd_set_dc_cs(bool dc,bool cs){
  gpio_put_masked((1u<<PIN_DC)|(1u<<PIN_CS),!!dc<<PIN_DC | !!cs<<PIN_CS);
}

static inline void lcd_write_cmd(PIO pio,uint sm,const uint8_t *cmd,size_t count){
  st7735_lcd_wait_idle(pio,sm);
  lcd_set_dc_cs(0,0);
  st7735_lcd_put(pio,sm,*cmd++);
  if(count>=2){
    st7735_lcd_wait_idle(pio,sm);
    lcd_set_dc_cs(1,0);
    for(size_t i=0;i<count-1;++i) st7735_lcd_put(pio,sm,*cmd++);
  }
  st7735_lcd_wait_idle(pio,sm);
  lcd_set_dc_cs(1,1);
}

static inline void lcd_init(PIO pio,uint sm,const uint8_t *init_seq){
  const uint8_t *cmd=init_seq;
  while(*cmd){
    lcd_write_cmd(pio,sm,cmd+2,*cmd);
    sleep_ms(*(cmd+1)*5);
    cmd+=*cmd+2;
  }
}

static inline void st7735_start_pixels(PIO pio,uint sm){
  uint8_t cmd=0x2c;
  lcd_write_cmd(pio,sm,&cmd,1);
  lcd_set_dc_cs(1,0);
}

static uint16_t framebuffer[HEIGHT][WIDTH];

const float ALPHA = 0.98f;
const float GYRO_SCALE_DPS = 1.0f;
const float ACCEL_SCALE_G = 1.0f;
const float DT_MIN_MS = 1.0f;
const float SCALE_CUBE = 7.0f;
const float CAM_DISTANCE = 80.0f;

static float estRoll = 0.0f, estPitch = 0.0f, estYaw = 0.0f;
static uint32_t last_t_us = 0;

const float cubeVerts[8][3] = {
  {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
  {0.5f,  0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
  {-0.5f, -0.5f,  0.5f}, {0.5f, -0.5f,  0.5f},
  {0.5f,  0.5f,  0.5f}, {-0.5f, 0.5f,  0.5f}
};

const uint8_t cubeEdges[][2] = {
  {0,1},{1,2},{2,3},{3,0},
  {4,5},{5,6},{6,7},{7,4},
  {0,4},{1,5},{2,6},{3,7}
};
const int EDGE_COUNT = 12;

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b){
  return ( ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3) );
}

void fb_clear(uint16_t color=0){
  for(int j=0;j<HEIGHT;j++) for(int i=0;i<WIDTH;i++) framebuffer[j][i]=color;
}

void fb_setPixel(int x,int y,uint16_t color){
  if(x<0||x>=WIDTH||y<0||y>=HEIGHT) return;
  framebuffer[y][x] = color;
}

void fb_drawLine(int x0,int y0,int x1,int y1,uint16_t color){
  int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
  int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
  int err = dx + dy;
  while(true){
    fb_setPixel(x0,y0,color);
    if(x0==x1 && y0==y1) break;
    int e2 = 2*err;
    if(e2 >= dy){ err += dy; x0 += sx; }
    if(e2 <= dx){ err += dx; y0 += sy; }
  }
}

void projectPoint(const float v[3], int &outx, int &outy, float scale=SCALE_CUBE){

  float z = v[2] + CAM_DISTANCE;
  if(z==0) z = 0.0001f;
  float px = (v[0] * scale) * (CAM_DISTANCE / z);
  float py = (v[1] * scale) * (CAM_DISTANCE / z);
  outx = (int)(WIDTH/2 + px);
  outy = (int)(HEIGHT/2 - py);
}

void rotatePoint(const float in[3], float out[3], float roll, float pitch, float yaw){

  float sr = sinf(roll), cr = cosf(roll);
  float sp = sinf(pitch), cp = cosf(pitch);
  float sy = sinf(yaw), cy = cosf(yaw);

  float x1 = in[0];
  float y1 = cr*in[1] - sr*in[2];
  float z1 = sr*in[1] + cr*in[2];

  float x2 = cp*x1 + sp*z1;
  float y2 = y1;
  float z2 = -sp*x1 + cp*z1;

  float x3 = cy*x2 - sy*y2;
  float y3 = sy*x2 + cy*y2;
  float z3 = z2;
  out[0]=x3; out[1]=y3; out[2]=z3;
}

void fb_flush_to_st7735(PIO pio, uint sm){
  st7735_start_pixels(pio,sm);
  for(int y=0;y<HEIGHT;y++){
    for(int x=0;x<WIDTH;x++){
      uint16_t c = framebuffer[y][x];
      st7735_lcd_put(pio,sm,c>>8);
      st7735_lcd_put(pio,sm,c&0xFF);
    }
  }
}

void updateOrientationFromIMU(float dt){
  float gx=0,gy=0,gz=0;
  float ax=0,ay=0,az=0;
  float mx=0,my=0,mz=0;
  bool hasGyro = ICM20948::icm20948GyroRead(&gx,&gy,&gz);
  bool hasAccel = ICM20948::icm20948AccelRead(&ax,&ay,&az);
  bool hasMag = ICM20948::icm20948MagRead(&mx,&my,&mz);

  gx *= GYRO_SCALE_DPS; gy *= GYRO_SCALE_DPS; gz *= GYRO_SCALE_DPS;

  const float DEG2RAD = 3.14159265358979f / 180.0f;
  float gx_r = gx * DEG2RAD;
  float gy_r = gy * DEG2RAD;
  float gz_r = gz * DEG2RAD;

  estRoll  += gx_r * dt;
  estPitch += gy_r * dt;
  estYaw   += gz_r * dt;

  if(hasAccel){
    float norm = sqrtf(ax*ax + ay*ay + az*az);
    if(norm>0.0001f){
      ax/=norm; ay/=norm; az/=norm;
      float roll_acc  = atan2f(ay, az);
      float pitch_acc = atan2f(-ax, sqrtf(ay*ay + az*az));
      estRoll  = ALPHA * estRoll  + (1.0f - ALPHA) * roll_acc;
      estPitch = ALPHA * estPitch + (1.0f - ALPHA) * pitch_acc;
    }
  }

  if(hasMag){
    float cosR = cosf(estRoll), sinR = sinf(estRoll);
    float cosP = cosf(estPitch), sinP = sinf(estPitch);
    float Xh = mx * cosP + my * sinR * sinP + mz * cosR * sinP;
    float Yh = my * cosR - mz * sinR;
    float yaw_mag = atan2f(-Yh, Xh);
    estYaw = ALPHA * estYaw + (1.0f - ALPHA) * yaw_mag;
  }
}

void setup(){

  seed_random_from_rosc();

  st7735_lcd_program_init(pio,sm,offset,PIN_DIN,PIN_CLK,SERIAL_CLK_DIV);
  gpio_init(PIN_CS);gpio_init(PIN_DC);gpio_init(PIN_RESET);
  gpio_set_dir(PIN_CS,GPIO_OUT);gpio_set_dir(PIN_DC,GPIO_OUT);gpio_set_dir(PIN_RESET,GPIO_OUT);
  gpio_put(PIN_CS,1);gpio_put(PIN_RESET,1);
  lcd_init(pio,sm,st7735_init_seq);

  ICM20948::imuInit(&enMotionSensorType);

  last_t_us = time_us_32();

}

void loop(){

  uint32_t now = time_us_32();
  uint32_t delta_us = now - last_t_us;
  if(delta_us < 1) delta_us = 1;
  float dt = (float)delta_us / 1000000.0f; // sekundy
  last_t_us = now;

  updateOrientationFromIMU(dt);

  fb_clear( rgb565(0,0,0) );

  float rotated[8][3];
  for(int i=0;i<8;i++){
    float sc = SCALE_CUBE;
    float in[3] = { cubeVerts[i][0]*SCALE_CUBE, cubeVerts[i][1]*SCALE_CUBE, cubeVerts[i][2]*SCALE_CUBE };
    rotatePoint(in, rotated[i], estRoll, estPitch, estYaw);
  }

  for(int e=0;e<EDGE_COUNT;e++){
    int a = cubeEdges[e][0];
    int b = cubeEdges[e][1];
    int x0,y0,x1,y1;
    projectPoint(rotated[a], x0, y0, SCALE_CUBE);
    projectPoint(rotated[b], x1, y1, SCALE_CUBE);
    fb_drawLine(x0,y0,x1,y1, rgb565(255,255,255));
  }

  fb_flush_to_st7735(pio,sm);

  sleep_ms(5);

}