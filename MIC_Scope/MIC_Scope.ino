// PDM I2S microphone oscilloscope // 

#include "hardware/structs/rosc.h"
#include "arducam_mic.h"
#include "st7735_lcd.pio.h"

#define PIN_DIN   11
#define PIN_CLK   10
#define PIN_CS    13
#define PIN_DC    9
#define PIN_RESET 7

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)
#define RESPON  1024
#define OFFSET  -50
#define SAMPLES 256

int16_t g_buffer[SAMPLES];
int16_t s_buffer[SAMPLES];
uint16_t col[SCR];

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

static inline void seed_random_from_rosc() {
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


void setup() {

  seed_random_from_rosc();

  config.data_buf = g_buffer;
  config.data_buf_size = SAMPLES;
  mic_i2s_init(&config);

  st7735_lcd_program_init(pio, sm, offset, PIN_DIN, PIN_CLK, 1.f);

  gpio_init(PIN_CS); gpio_set_dir(PIN_CS, GPIO_OUT); gpio_put(PIN_CS, 1);
  gpio_init(PIN_DC); gpio_set_dir(PIN_DC, GPIO_OUT); gpio_put(PIN_DC, 1);
  gpio_init(PIN_RESET); gpio_set_dir(PIN_RESET, GPIO_OUT); gpio_put(PIN_RESET, 1);

  lcd_init(pio, sm, st7735_init_seq);
    
}

void loop() {

  mic_safe_snapshot(s_buffer, SAMPLES);

  memset(col, 0, 2*SCR);

  int samples_per_pixel = SAMPLES / HEIGHT;

  for (int y = 0; y < HEIGHT; y++) {

    int32_t sum = 0;
    for (int i = 0; i < samples_per_pixel; i++) sum += s_buffer[y * samples_per_pixel + i];

    int16_t avg = sum / samples_per_pixel;

    int x = OFFSET + (avg * (WIDTH - 1)) / RESPON;
    if (x < 0) x = 0;
    if (x >= WIDTH) x = WIDTH - 1;

    col[x + y * WIDTH]  = 65535;
            
  }

  st7735_start_pixels(pio, sm);

  for (int i = 0; i < SCR; i++) {

    uint16_t coll = col[i];
    st7735_lcd_put(pio, sm, coll >> 8);
    st7735_lcd_put(pio, sm, coll & 0xff);

  }

}