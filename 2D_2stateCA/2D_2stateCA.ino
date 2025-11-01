// 2D Cellular automata 2 states //

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

#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define WIDTH   80
#define HEIGHT  160
#define SCR     WIDTH*HEIGHT

  uint8_t grid[WIDTH][HEIGHT];
  uint8_t new_grid[WIDTH][HEIGHT];
  uint16_t color;

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
  randomSeed(random);

}

void rndrule(){

  for (int y = 0; y < HEIGHT; y++) {

    for (int x = 0; x < WIDTH; x++) grid[x][y] = random(2);

  }

}

uint8_t count_neighbors(uint8_t x, uint8_t y, uint8_t state) {

  uint8_t count = 0;
  for (int8_t i = -1; i <= 1; i++) {
    for (int8_t j = -1; j <= 1; j++) {
      if (i == 0 && j == 0) continue;
      uint8_t nx = (x + i + WIDTH) % WIDTH;
      uint8_t ny = (y + j + HEIGHT) % HEIGHT;
      if (grid[nx][ny] == state) count++;
    }
  }

  return count;

}

void update_grid() {

  for (uint8_t y = 0; y < HEIGHT; y++) {

    for (uint8_t x = 0; x < WIDTH; x++) {

      uint8_t current_state = grid[x][y];
      uint8_t neighbors = count_neighbors(x, y, 1);
      new_grid[x][y] = current_state;
      if (current_state == 1) {
        if (neighbors < 2 || neighbors > 3) new_grid[x][y] = 0;
      } else {
        if (neighbors == 3) new_grid[x][y] = 1;
      }

    }

  }

  for (uint8_t y = 0; y < HEIGHT; y++) {

    for (uint8_t x = 0; x < WIDTH; x++) grid[x][y] = new_grid[x][y];

  }

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
  
  st7735_start_pixels(pio, sm);

  update_grid();

  for (int y = 0; y < HEIGHT; y++) {

    for (int x = 0; x < WIDTH; x++) {

      if (grid[x][y] == 0) color = BLACK;
      if (grid[x][y] == 1) color = WHITE;
      st7735_lcd_put(pio, sm, color >> 8);
      st7735_lcd_put(pio, sm, color & 0xff);

    }
  
  }
 
}