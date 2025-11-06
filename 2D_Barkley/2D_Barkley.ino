// 2D Barkley reaction-diffusion model //

#include "hardware/structs/rosc.h"
#include "st7735_lcd.pio.h"

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

#define DT      0.208f
#define DX      1.0f
#define A       1.145f
#define B       0.061f
#define EPSILON 0.075f
#define DU      1.36f
#define DV      0.435f

float u[WIDTH][HEIGHT];
float v[WIDTH][HEIGHT];
float u_new[WIDTH][HEIGHT];
float v_new[WIDTH][HEIGHT];

float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);}

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

void init_grid(){
  for(int y=0;y<HEIGHT;y++){
    for(int x=0;x<WIDTH;x++){
      u[x][y] = randomf(0.0f, 1.0f);
      v[x][y] = randomf(0.0f, 0.5f);
    }
  }
}

float laplace_u(int x,int y){
  float sum = -4.0f*u[x][y];
  sum += u[(x+1)%WIDTH][y];
  sum += u[(x-1+WIDTH)%WIDTH][y];
  sum += u[x][(y+1)%HEIGHT];
  sum += u[x][(y-1+HEIGHT)%HEIGHT];
  return sum/(DX*DX);
}

float laplace_v(int x,int y){
  float sum = -4.0f * v[x][y];
  sum += v[(x+1)%WIDTH][y];
  sum += v[(x-1+WIDTH)%WIDTH][y];
  sum += v[x][(y+1)%HEIGHT];
  sum += v[x][(y-1+HEIGHT)%HEIGHT];
  return sum / (DX*DX);
}

void update_grid() {

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {

      float Lu = laplace_u(x, y);
      float Lv = laplace_v(x, y);
      float reaction = u[x][y] * (1.0f - u[x][y]) * (u[x][y] - (v[x][y] + B) / A);
      float du = DU * Lu + (reaction / EPSILON);
      float dv = DV * Lv + (u[x][y] - v[x][y]);

      u_new[x][y] = u[x][y] + du * DT;
      v_new[x][y] = v[x][y] + dv * DT;

      if (u_new[x][y] < 0) u_new[x][y] = 0;
      if (u_new[x][y] > 1) u_new[x][y] = 1;
      if (v_new[x][y] < 0) v_new[x][y] = 0;
      if (v_new[x][y] > 1) v_new[x][y] = 1;

    }
  }

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      u[x][y] = u_new[x][y];
      v[x][y] = v_new[x][y];
    }
  }
}

uint16_t u_to_color(float val){
  uint8_t hue = (uint8_t)(val*255);
  uint8_t r=0,g=0,b=0;
  int region = hue/43;
  int remainder = (hue%43)*6;
  int p=0, q=255-remainder, t=remainder;
  switch(region){
    case 0: r=255; g=t; b=0; break;
    case 1: r=q; g=255; b=0; break;
    case 2: r=0; g=255; b=t; break;
    case 3: r=0; g=q; b=255; break;
    case 4: r=t; g=0; b=255; break;
    case 5: r=255; g=0; b=q; break;
  }
  return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}

void setup(){

  seed_random_from_rosc();

  st7735_lcd_program_init(pio,sm,offset,PIN_DIN,PIN_CLK,SERIAL_CLK_DIV);
  gpio_init(PIN_CS);gpio_init(PIN_DC);gpio_init(PIN_RESET);
  gpio_set_dir(PIN_CS,GPIO_OUT);gpio_set_dir(PIN_DC,GPIO_OUT);gpio_set_dir(PIN_RESET,GPIO_OUT);
  gpio_put(PIN_CS,1);gpio_put(PIN_RESET,1);
  lcd_init(pio,sm,st7735_init_seq);

  init_grid();

}

void loop(){

  st7735_start_pixels(pio,sm);

  update_grid();

  for(int y=0;y<HEIGHT;y++){

    for(int x=0;x<WIDTH;x++){

      uint16_t coll = u_to_color(u[x][y]);
      st7735_lcd_put(pio,sm,coll>>8);
      st7735_lcd_put(pio,sm,coll&0xFF);

    }

  }

}