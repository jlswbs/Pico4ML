// 2D random rule life CA //

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

#define ALIVE 1
#define DEAD  0

static uint8_t current[HEIGHT][WIDTH];
static uint8_t next[HEIGHT][WIDTH];
static uint8_t age[HEIGHT][WIDTH];

static uint8_t survival_rules[9];
static uint8_t birth_rules[9];

static void choose_unique_flags(uint8_t flags[9], int k) {
  for(int i=0;i<9;i++) flags[i]=0;
  int chosen=0;
  while(chosen<k){
    int r = rand()%9;
    if(!flags[r]) {flags[r]=1; chosen++;}
  }
}

static void rndrule() {
  int surv_k = 1 + rand()%9;
  int birth_k = 1 + rand()%9;
  choose_unique_flags(survival_rules, surv_k);
  choose_unique_flags(birth_rules, birth_k);

  for(int y=0;y<HEIGHT;y++){
    for(int x=0;x<WIDTH;x++){
      current[y][x] = (rand()%100)<20 ? ALIVE : DEAD;
      age[y][x] = current[y][x]==ALIVE ? 1 : 0;
    }
  }
}

static inline int wrapi(int v, int m){
  if(v<0) return v+m;
  if(v>=m) return v-m;
  return v;
}

static void step() {
  for(int y=0;y<HEIGHT;y++){
    for(int x=0;x<WIDTH;x++){
      int count=0;
      for(int ny=y-1;ny<=y+1;ny++){
        for(int nx=x-1;nx<=x+1;nx++){
          if(nx==x && ny==y) continue;
          int wx = wrapi(nx, WIDTH);
          int wy = wrapi(ny, HEIGHT);
          if(current[wy][wx]==ALIVE) count++;
        }
      }

      if(current[y][x]==ALIVE){
        next[y][x] = survival_rules[count] ? ALIVE : DEAD;
      } else {
        next[y][x] = birth_rules[count] ? ALIVE : DEAD;
      }

      if(next[y][x]==ALIVE){
        age[y][x] = current[y][x]==ALIVE ? age[y][x]+1 : 1;
      } else {
        age[y][x] = 0;
      }
    }
  }

  for(int y=0;y<HEIGHT;y++)
    for(int x=0;x<WIDTH;x++){
      current[y][x] = next[y][x];
    }
}

static inline uint16_t color_for_cell(uint8_t val, uint16_t cell_age){
  if(val==DEAD) return 0x0000;
  if(cell_age < 5) return 0xFFFF;
  else if(cell_age < 15) return 0xFFE0;
  else if(cell_age < 30) return 0xF800;
  else return 0xF810;
}

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
  for (int k = 0; k < 32; k++) {
    while (1) {
      random_bit = (*rnd_reg) & 1;
      if (random_bit != ((*rnd_reg) & 1)) break;
    }
    random = (random << 1) | random_bit;
  }
  srand(random);

}

void setup() {

  st7735_lcd_program_init(pio, sm, offset, PIN_DIN, PIN_CLK, SERIAL_CLK_DIV);

  gpio_init(PIN_CS);
  gpio_init(PIN_DC);
  gpio_init(PIN_RESET);
  gpio_set_dir(PIN_CS, GPIO_OUT);
  gpio_set_dir(PIN_DC, GPIO_OUT);
  gpio_set_dir(PIN_RESET, GPIO_OUT);
  gpio_put(PIN_CS, 1);
  gpio_put(PIN_RESET, 1);

  lcd_init(pio, sm, st7735_init_seq);

  seed_random_from_rosc();

  rndrule();

}

void loop() {

  step();

  st7735_start_pixels(pio, sm);

  for(int y=0;y<HEIGHT;y++){
    for(int x=0;x<WIDTH;x++){
      uint16_t c = color_for_cell(current[y][x], age[y][x]);
      st7735_lcd_put(pio, sm, c>>8);
      st7735_lcd_put(pio, sm, c&0xFF);
    }
  }

  delay(50);

}