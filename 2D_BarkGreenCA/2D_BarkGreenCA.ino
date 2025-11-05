// 2D Greenberg-Hastings-Barkley continuous cellular automaton //

#include "hardware/structs/rosc.h"
#include "st7735_lcd.pio.h"

#define PIN_DIN   11
#define PIN_CLK   10
#define PIN_CS    13
#define PIN_DC    9
#define PIN_RESET 7

#define WIDTH   80
#define HEIGHT  160

#define STATES 10
#define THRESH 2
#define P_INIT 6
#define ANGLE_DRIFT 1

uint8_t grid[WIDTH][HEIGHT];
uint8_t new_grid[WIDTH][HEIGHT];
uint8_t hue[WIDTH][HEIGHT];
uint8_t new_hue[WIDTH][HEIGHT];

int8_t dirx8[8] = {1, 1, 0, -1, -1, -1, 0, 1};
int8_t diry8[8] = {0, 1, 1, 1, 0, -1, -1, -1};

static inline uint16_t hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v) {
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;

    uint16_t p = (v * (255 - s)) >> 8;
    uint16_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint16_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    uint8_t r, g, b;
    switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#define SERIAL_CLK_DIV 1.f

PIO pio = pio0;
uint sm = 0;
uint offset = pio_add_program(pio, &st7735_lcd_program);

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
    for (int x = 0; x < WIDTH; x++) {
      grid[x][y] = (rand() % 100 < P_INIT) ? 1 : 0;
      hue[x][y] = rand() & 0xFF;
    }
  }

}

void update_grid() {

  for (uint8_t y = 0; y < HEIGHT; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {

      uint8_t s = grid[x][y];
      uint8_t ns = s;
      uint8_t nh = hue[x][y];

      if (s == 0) {
        int sumx = 0;
        int sumy = 0;
        uint8_t count = 0;
        for (uint8_t d = 0; d < 8; d++) {
          int nx = (x + dirx8[d] + WIDTH) % WIDTH;
          int ny = (y + diry8[d] + HEIGHT) % HEIGHT;
          if (grid[nx][ny] == 1) {
            count++;
            uint8_t h = hue[nx][ny];
            float ang = ((float)h) * (2.0f * M_PI / 256.0f);
            sumx += (int)roundf(cosf(ang) * 256.0f);
            sumy += (int)roundf(sinf(ang) * 256.0f);
          }
        }
        if (count >= THRESH) {
          ns = 1;
          if (sumx == 0 && sumy == 0) nh = rand() & 0xFF;
          else {
            float avg_ang = atan2f((float)sumy, (float)sumx);
            uint8_t mapped = (uint8_t)((avg_ang + M_PI) * (256.0f / (2.0f * M_PI)));
            nh = mapped;
          }
        }
      } else if (s == 1) {
        ns = 2;
      } else {
        ns = (s + 1) % STATES;
        nh = (uint8_t)(((int)hue[x][y] + ANGLE_DRIFT) & 0xFF);
      }

      new_grid[x][y] = ns;
      new_hue[x][y] = nh;
    }
  }

  for (uint8_t y = 0; y < HEIGHT; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      grid[x][y] = new_grid[x][y];
      hue[x][y] = new_hue[x][y];
    }
  }

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

      uint8_t s = grid[x][y];
      uint8_t h = hue[x][y];
      uint16_t color;
      if (s == 0) {
        color = 0x0011;
      } else {
        uint8_t v = 255 - (uint8_t)(((uint16_t)(s - 1) * 200) / (STATES - 1));
        if (v < 40) v = 40;
        color = hsv_to_rgb(h, 255, v);
      }

      st7735_lcd_put(pio, sm, color >> 8);
      st7735_lcd_put(pio, sm, color & 0xff);
      
    }

  }

}