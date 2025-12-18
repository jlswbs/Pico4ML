// 2D random rule excitable CA //

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
#define SERIAL_CLK_DIV 1.f

#define WIDTH   80
#define HEIGHT  160

#define ALIVE     3
#define DEATH_1   2
#define DEATH_2   1
#define DEAD      0

static uint8_t grid[HEIGHT][WIDTH];
static uint8_t next_grid[HEIGHT][WIDTH];
static uint8_t age[HEIGHT][WIDTH];

static uint8_t survive[9];
static uint8_t born[9];

static inline int wrap(int v, int m) {
  if (v < 0) return v + m;
  if (v >= m) return v - m;
  return v;
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

static inline uint16_t color_for(uint8_t v, uint8_t a) {
  if (v == DEAD) return rgb565(0, 0, 0);
  if (v == ALIVE) {
    if (a > 50) return rgb565(255, 0, 0);
    if (a > 2)  return rgb565(255, 255, 0);
    return rgb565(255, 255, 255);
  }
  if (v == DEATH_1) return rgb565(0, 0, 255);
  return rgb565(32, 32, 32);
}

static void random_rules() {
  for (int i = 0; i < 9; i++) {
    survive[i] = rand() & 1;
    born[i]    = rand() & 1;
  }
}

static void random_fill() {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      grid[y][x] = (rand() % 100 < 20) ? ALIVE : DEAD;
      age[y][x] = 0;
    }
  }
}

static void step() {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {

      int n = 0;
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          if (!dx && !dy) continue;
          if (grid[wrap(y + dy, HEIGHT)][wrap(x + dx, WIDTH)] == ALIVE)
            n++;
        }
      }

      uint8_t v = grid[y][x];

      if (v == ALIVE) {
        next_grid[y][x] = survive[n] ? ALIVE : DEATH_1;
        age[y][x] = survive[n] ? (age[y][x] + 1) : 0;
      }
      else if (v == DEAD) {
        next_grid[y][x] = born[n] ? ALIVE : DEAD;
        age[y][x] = 0;
      }
      else {
        next_grid[y][x] = v - 1;
        age[y][x] = 0;
      }
    }
  }

  memcpy(grid, next_grid, sizeof(grid));
}

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
    for (size_t i = 0; i < count - 1; ++i)
      st7735_lcd_put(pio, sm, *cmd++);
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

  random_rules();
  random_fill();

}

void loop() {

  step();

  st7735_start_pixels(pio, sm);

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      uint16_t c = color_for(grid[y][x], age[y][x]);
      st7735_lcd_put(pio, sm, c >> 8);
      st7735_lcd_put(pio, sm, c & 0xFF);
    }
  }

  delay(20);

}