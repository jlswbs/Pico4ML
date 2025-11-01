// 2D Cellular automata N-states //

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

#define WIDTH   80
#define HEIGHT  160
#define SCR     WIDTH*HEIGHT
#define SERIAL_CLK_DIV 1.f

#define STATES        12
#define INIT_DENSITY  95

uint8_t grid[WIDTH][HEIGHT];
uint8_t newGrid[WIDTH][HEIGHT];
uint16_t palette[STATES];
uint16_t color;

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

static inline void lcd_set_dc_cs(bool dc, bool cs){
  gpio_put_masked((1u<<PIN_DC)|(1u<<PIN_CS), !!dc<<PIN_DC | !!cs<<PIN_CS);
}

static inline void lcd_write_cmd(PIO pio, uint sm, const uint8_t *cmd, size_t count){
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

static inline void lcd_init(PIO pio, uint sm, const uint8_t *init_seq){
  const uint8_t *cmd = init_seq;
  while (*cmd) {
    lcd_write_cmd(pio, sm, cmd + 2, *cmd);
    sleep_ms(*(cmd + 1) * 5);
    cmd += *cmd + 2;
  }
}

static inline void st7735_start_pixels(PIO pio, uint sm){
  uint8_t cmd = 0x2c;
  lcd_write_cmd(pio, sm, &cmd, 1);
  lcd_set_dc_cs(1, 0);
}

static inline void seed_random_from_rosc(){
  uint32_t random = 0, bit;
  volatile uint32_t *rnd_reg = (uint32_t*)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
  for (int k = 0; k < 32; k++) {
    while (1) {
      bit = (*rnd_reg) & 1;
      if (bit != ((*rnd_reg) & 1)) break;
    }
    random = (random << 1) | bit;
  }
  srand(random);
  randomSeed(random);
}

uint16_t hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v){
  float hf = h / 255.0f * 6.0f;
  int i = (int)hf;
  float f = hf - i;
  float p = v * (1.0f - s / 255.0f);
  float q = v * (1.0f - f * s / 255.0f);
  float t = v * (1.0f - (1.0f - f) * s / 255.0f);
  float r,g,b;
  switch(i % 6){
    case 0: r=v; g=t; b=p; break;
    case 1: r=q; g=v; b=p; break;
    case 2: r=p; g=v; b=t; break;
    case 3: r=p; g=q; b=v; break;
    case 4: r=t; g=p; b=v; break;
    default:r=v; g=p; b=q; break;
  }
  uint16_t R=(uint8_t)r, G=(uint8_t)g, B=(uint8_t)b;
  return ((R&0xF8)<<8)|((G&0xFC)<<3)|(B>>3);
}

void generate_palette(){
  for (int i=0;i<STATES;i++){
    uint8_t h = (uint8_t)(255.0f * i / STATES);
    palette[i] = hsv_to_rgb(h, 255, 255);
  }
}

void rndrule(){
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      grid[x][y] = (rand()%100 < INIT_DENSITY) ? rand()%STATES : 0;
    }
  }
}

void updateGrid(){
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      int s = grid[x][y];
      int neighborCounts[STATES];
      for (int i=0;i<STATES;i++) neighborCounts[i]=0;

      for (int dx=-1; dx<=1; dx++) {
        for (int dy=-1; dy<=1; dy++) {
          if (dx==0 && dy==0) continue;
          int nx=(x+dx+WIDTH)%WIDTH;
          int ny=(y+dy+HEIGHT)%HEIGHT;
          neighborCounts[grid[nx][ny]]++;
        }
      }

      int next = (s + 1) % STATES;
      int prev = (s - 1 + STATES) % STATES;

      if (neighborCounts[next] >= 3) newGrid[x][y] = next;
      else if (neighborCounts[s] < 2) newGrid[x][y] = prev;
      else newGrid[x][y] = s;
    }
  }

  for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < WIDTH; x++)
      grid[x][y] = newGrid[x][y];
}

void setup(){

  st7735_lcd_program_init(pio, sm, offset, PIN_DIN, PIN_CLK, SERIAL_CLK_DIV);
  gpio_init(PIN_CS); gpio_init(PIN_DC); gpio_init(PIN_RESET);
  gpio_set_dir(PIN_CS, GPIO_OUT); gpio_set_dir(PIN_DC, GPIO_OUT); gpio_set_dir(PIN_RESET, GPIO_OUT);
  gpio_put(PIN_CS, 1); gpio_put(PIN_RESET, 1);
  lcd_init(pio, sm, st7735_init_seq);

  seed_random_from_rosc();

  generate_palette();

  rndrule();

}

void loop(){

  st7735_start_pixels(pio, sm);

  updateGrid();

  for (int y=0;y<HEIGHT;y++){

    for (int x=0;x<WIDTH;x++){

      color = palette[grid[x][y]];
      st7735_lcd_put(pio, sm, color>>8);
      st7735_lcd_put(pio, sm, color&0xff);
      
    }

  }

}