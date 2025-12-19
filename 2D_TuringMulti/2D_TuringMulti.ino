// Multi-Scale Turing patterns // 

#include "hardware/structs/rosc.h"
#include "st7735_lcd.pio.h"

#define PIN_DIN   11
#define PIN_CLK   10
#define PIN_CS    13
#define PIN_DC    9
#define PIN_RESET 7

PIO pio = pio0;
uint sm = 0;
uint offset = pio_add_program(pio, &st7735_lcd_program);

#define WIDTH   60
#define HEIGHT  140
#define FULLW   80
#define FULLH   160
#define SCR     (WIDTH*HEIGHT)
#define SERIAL_CLK_DIV 1.f

static const uint8_t st7735_init_seq[] = {
  1, 20, 0x01,
  1, 10, 0x11,
  2, 2, 0x3A, 0x05,
  2, 0, 0x36, 0x08,
  5, 0, 0x2A, 0x00, 0x18, 0x00, 0x67,
  5, 0, 0x2B, 0x00, 0x00, 0x00, 0x9F,
  1, 2, 0x20,
  1, 2, 0x13,
  1, 2, 0x29,
  0
};

static inline void lcd_set_dc_cs(bool dc, bool cs) {
  sleep_us(1);
  gpio_put_masked((1u << PIN_DC) | (1u << PIN_CS), !!dc << PIN_DC | !!cs << PIN_CS);
  sleep_us(1);
}

static inline void lcd_write_cmd(PIO pio, uint sm, const uint8_t *cmd, size_t count) {
  st7735_lcd_wait_idle(pio, sm);
  lcd_set_dc_cs(0, 0);
  st7735_lcd_put(pio, sm, *cmd++);
  if (count >= 2) {
    st7735_lcd_wait_idle(pio, sm);
    lcd_set_dc_cs(1, 0);
    for (size_t i = 0; i < count - 1; ++i) st7735_lcd_put(pio, sm, *cmd++);
  }
  st7735_lcd_wait_idle(pio, sm);
  lcd_set_dc_cs(1, 1);
}

static inline void lcd_init(PIO pio, uint sm, const uint8_t *init_seq) {
  const uint8_t *cmd = init_seq;
  while (*cmd) {
    lcd_write_cmd(pio, sm, cmd + 2, *cmd);
    sleep_ms(*(cmd + 1) * 5);
    cmd += *cmd + 2;
  }
}

static inline void st7735_start_pixels(PIO pio, uint sm) {
  uint8_t cmd = 0x2c;
  lcd_write_cmd(pio, sm, &cmd, 1);
  lcd_set_dc_cs(1, 0);
}

static inline void seed_random_from_rosc(){
    uint32_t random = 0;
    uint32_t random_bit;
    volatile uint32_t *rnd_reg = (uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
    for(int k=0;k<32;k++){
        while(1){
            random_bit = (*rnd_reg)&1;
            if(random_bit != ((*rnd_reg)&1)) break;
        }
        random = (random<<1)|random_bit;
    }
    srand(random);
}

  float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);} 
  
  int level, radius, i, x, y;
  int blurlevels;
  float base;
  int levels;
  float stepScale;
  float stepOffset;
  float blurFactor;

  int radii[WIDTH];
  float stepSizes[WIDTH];

  float grid[SCR];
  float blurBuffer[SCR];
  float variation[SCR];
  float bestVariation[SCR];
  int bestLevel[SCR];
  bool direction[SCR];

  float activator[SCR];
  float inhibitor[SCR];
  float swap[SCR];
    
void rndrule() {

  base = randomf(1.4f, 2.1f);
  stepScale = randomf(0.01f, 0.2f);
  stepOffset = randomf(0.01f, 0.4f);
  blurFactor = randomf(0.5f, 1.0f);

  levels = (int) (log(max(WIDTH,HEIGHT)) / logf(base)) - 1.0f;
  blurlevels = (int) max(0, (levels+1) * blurFactor - 0.5f);

  for (i = 0; i < levels; i++) {
    int radius = (int)powf(base, i);
    radii[i] = radius;
    stepSizes[i] = logf(radius) * stepScale + stepOffset;
  }

  for (i = 0; i < SCR; i++) grid[i] = randomf(-1.0f, 1.0f);

}

uint16_t color_from_float(float v){

  uint8_t val = (uint8_t)((v + 1.0f) * 127.0f);
  return ((val >> 3) << 11) | ((val >> 2) << 5) | (val >> 3);

}

void setup(){

  st7735_lcd_program_init(pio, sm, offset, PIN_DIN, PIN_CLK, SERIAL_CLK_DIV);

  gpio_init(PIN_CS); gpio_init(PIN_DC); gpio_init(PIN_RESET);
  gpio_set_dir(PIN_CS, GPIO_OUT); gpio_set_dir(PIN_DC, GPIO_OUT); gpio_set_dir(PIN_RESET, GPIO_OUT);
  gpio_put(PIN_CS,1); gpio_put(PIN_RESET,1);

  lcd_init(pio, sm, st7735_init_seq);

  seed_random_from_rosc();

  rndrule();

}

void loop(){

  memcpy(activator, grid, 4*SCR);

  for (level = 0; level < levels - 1; level++) {

    int radius = radii[level];

    if(level <= blurlevels){
        
      for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
          int t = y * WIDTH + x;
          if (y == 0 && x == 0) blurBuffer[t] = activator[t];
          else if (y == 0) blurBuffer[t] = blurBuffer[t - 1] + activator[t];
          else if (x == 0) blurBuffer[t] = blurBuffer[t - WIDTH] + activator[t];
          else blurBuffer[t] = blurBuffer[t - 1] + blurBuffer[t - WIDTH] - blurBuffer[t - WIDTH - 1] + activator[t];
        }
      }
    }
    
    for (y = 0; y < HEIGHT; y++) {
      for (x = 0; x < WIDTH; x++) {
        int minx = max(0, x - radius);
        int maxx = min(x + radius, WIDTH - 1);
        int miny = max(0, y - radius);
        int maxy = min(y + radius, HEIGHT - 1);
        int area = (maxx - minx) * (maxy - miny);

        int nw = miny * WIDTH + minx;
        int ne = miny * WIDTH + maxx;
        int sw = maxy * WIDTH + minx;
        int se = maxy * WIDTH + maxx;

        int t = y * WIDTH + x;
        inhibitor[t] = (blurBuffer[se] - blurBuffer[sw] - blurBuffer[ne] + blurBuffer[nw]) / area;
      }
    }
        
    for (i = 0; i < SCR; i++) {
      float variation = fabs(activator[i] - inhibitor[i]);
      if (level == 0 || variation < bestVariation[i]) {
        bestVariation[i] = variation;
        bestLevel[i] = level;
        direction[i] = activator[i] > inhibitor[i];
      }
    }

    if(level==0) {
      memcpy(activator, inhibitor, 4*SCR);
    } else {
      memcpy(swap, activator, 4*SCR);
      memcpy(activator, inhibitor, 4*SCR);
      memcpy(inhibitor, swap, 4*SCR);
    }
            
  }

  float smallest = MAXFLOAT;
  float largest = -MAXFLOAT;

  for (i = 0; i < SCR; i++) {
    float curStep = stepSizes[bestLevel[i]];
    if (direction[i])grid[i] += curStep;
    else grid[i] -= curStep;
    smallest = min(smallest, grid[i]);
    largest = max(largest, grid[i]);
  }

  float range = (largest - smallest) / 2.0f;

  st7735_start_pixels(pio, sm);

  for(int i=0;i<SCR;i++) grid[i] = ((grid[i]-smallest)/range)-1.0f;

  for(int y_disp=0; y_disp<FULLH; y_disp++){
    int y_grid = y_disp * HEIGHT / FULLH;
    for(int x_disp=0; x_disp<FULLW; x_disp++){
      int x_grid = x_disp * WIDTH / FULLW;
      uint16_t c = color_from_float(grid[y_grid * WIDTH + x_grid]);
      st7735_lcd_put(pio, sm, c>>8);
      st7735_lcd_put(pio, sm, c&0xFF);
    }
  }

}