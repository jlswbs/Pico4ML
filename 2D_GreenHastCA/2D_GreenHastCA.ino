// 2D Greenberg-Hastings cellular automaton //

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

#define N_STATES 10
#define THRESHOLD 2
#define P_INIT 2

uint8_t grid[WIDTH][HEIGHT];
uint8_t new_grid[WIDTH][HEIGHT];
uint16_t colors[N_STATES];

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
  randomSeed(random);
}

void rndrule(){

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) grid[x][y] = (rand() % 100 < P_INIT) ? 1 : 0;
  }

  for (int i = 0; i < N_STATES; i++) {
    uint8_t r = (i * 31) / (N_STATES - 1);
    uint8_t g = (i * 63) / (N_STATES - 1);
    uint8_t b = 31 - (i * 31) / (N_STATES - 1);
    colors[i] = (r << 11) | (g << 5) | b;
  }

}

void update_grid() {

  for (uint8_t y = 0; y < HEIGHT; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      uint8_t current = grid[x][y];
      uint8_t excited_neighbors = 0;

      for (int8_t i = -1; i <= 1; i++) {
        for (int8_t j = -1; j <= 1; j++) {
          if (i == 0 && j == 0) continue;
          uint8_t nx = (x + i + WIDTH) % WIDTH;
          uint8_t ny = (y + j + HEIGHT) % HEIGHT;
          if (grid[nx][ny] == 1) excited_neighbors++;
        }
      }

      if (current == 0) {                   // klid
        if (excited_neighbors >= THRESHOLD) new_grid[x][y] = 1;
        else new_grid[x][y] = 0;
      } else {                              // excited nebo refrakterní
        new_grid[x][y] = current + 1;
        if (new_grid[x][y] >= N_STATES) new_grid[x][y] = 0;
      }
    }
  }

  for (uint8_t y = 0; y < HEIGHT; y++)
    for (uint8_t x = 0; x < WIDTH; x++)
      grid[x][y] = new_grid[x][y];

}


void setup() {

  seed_random_from_rosc();

  st7735_lcd_program_init(pio, sm, offset, PIN_DIN, PIN_CLK, 1.f);

  gpio_init(PIN_CS); gpio_set_dir(PIN_CS, GPIO_OUT); gpio_put(PIN_CS, 1);
  gpio_init(PIN_DC); gpio_set_dir(PIN_DC, GPIO_OUT); gpio_put(PIN_DC, 1);
  gpio_init(PIN_RESET); gpio_set_dir(PIN_RESET, GPIO_OUT); gpio_put(PIN_RESET, 1);

  lcd_init(pio, sm, st7735_init_seq);

  rndrule();

}

void loop() {

  st7735_start_pixels(pio, sm);

  update_grid();

  for (int y = 0; y < HEIGHT; y++) {

    for (int x = 0; x < WIDTH; x++) {

      uint16_t coll = colors[grid[x][y]];

      st7735_lcd_put(pio, sm, coll >> 8);
      st7735_lcd_put(pio, sm, coll & 0xff);

    }

  }

}