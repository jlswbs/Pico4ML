// PDM I2S microphone spectrum analyser // 

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
#define OFFSET  -60
#define SAMPLES 256

int16_t g_buffer[SAMPLES];
int16_t s_buffer[SAMPLES];
uint16_t col[SCR];

PIO pio = pio0;
uint sm = 0;
uint offset = pio_add_program(pio, &st7735_lcd_program);

#define FFT_N SAMPLES
#define FFT_HALF (FFT_N/2)
#define BANDS 40
#define BINS_PER_BAND (FFT_HALF / BANDS)

static float fft_re[FFT_N];
static float fft_im[FFT_N];
static float window_fn[FFT_N];
static float band_smoothed[BANDS];
static float max_db = -120.0f;
static const float floor_db = -65.0f;
static const float attack = 0.6f;
static const float decay = 0.98f;

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
}

static void fft_init_window() {
  for (int n = 0; n < FFT_N; ++n) {
    window_fn[n] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * n / (FFT_N - 1)));
  }
}

static void fft_bit_reverse(float *re, float *im) {
  int j = 0;
  for (int i = 0; i < FFT_N; ++i) {
    if (i < j) {
      float tr = re[i]; re[i] = re[j]; re[j] = tr;
      float ti = im[i]; im[i] = im[j]; im[j] = ti;
    }
    int m = FFT_N >> 1;
    while (m >= 1 && j >= m) {
      j -= m;
      m >>= 1;
    }
    j += m;
  }
}

static void fft_compute(float *re, float *im) {
  fft_bit_reverse(re, im);

  for (int len = 2; len <= FFT_N; len <<= 1) {
    float ang = -2.0f * (float)M_PI / len;
    float wlen_r = cosf(ang);
    float wlen_i = sinf(ang);
    for (int i = 0; i < FFT_N; i += len) {
      float ur = 1.0f;
      float ui = 0.0f;
      for (int j = 0; j < (len >> 1); ++j) {
        int u = i + j;
        int v = i + j + (len >> 1);
        float vr = re[v] * ur - im[v] * ui;
        float vi = re[v] * ui + im[v] * ur;
        re[v] = re[u] - vr;
        im[v] = im[u] - vi;
        re[u] += vr;
        im[u] += vi;
        float tmp_r = ur * wlen_r - ui * wlen_i;
        float tmp_i = ur * wlen_i + ui * wlen_r;
        ur = tmp_r;
        ui = tmp_i;
      }
    }
  }
}

static void compute_bands_from_snapshot(int16_t *snap) {

  for (int i = 0; i < FFT_N; ++i) {
    float v = (float)snap[i] / 65535.0f;
    fft_re[i] = v * window_fn[i];
    fft_im[i] = 0.0f;
  }

  fft_compute(fft_re, fft_im);

  float band_vals[BANDS];
  for (int b = 0; b < BANDS; ++b) band_vals[b] = 0.0f;

  for (int k = 1; k < FFT_HALF; ++k) {
    float re = fft_re[k];
    float im = fft_im[k];
    float mag = re * re + im * im;
    int band = (k - 1) / BINS_PER_BAND;
    if (band >= 0 && band < BANDS) band_vals[band] += mag;
  }

  for (int b = 0; b < BANDS; ++b) {
    float amp = sqrtf(band_vals[b]) + 1e-9f;
    float db = 20.0f * log10f(amp);
    if (!isfinite(db)) db = floor_db;

    if (db > max_db) max_db = db;
    else max_db = max_db * decay + db * (1.0f - decay);
    if (max_db < floor_db + 1.0f) max_db = floor_db + 1.0f;

    float norm = (db - floor_db) / (max_db - floor_db);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    float prev = band_smoothed[b];
    if (norm > prev) band_smoothed[b] = prev * (1.0f - attack) + norm * attack;
    else band_smoothed[b] = prev * 0.9f + norm * 0.1f;
  }

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

  fft_init_window();

}

void loop() {

  mic_safe_snapshot(s_buffer, SAMPLES);

  memset(col, 0, sizeof(col));

  compute_bands_from_snapshot(s_buffer);

  int band_height = HEIGHT / BANDS;

  for (int b = 1; b < BANDS; b++) {

    int bar_w = band_smoothed[b] * (WIDTH-1);
    if (bar_w > WIDTH) bar_w = WIDTH;

    for (int y = b * band_height; y < (b+1) * band_height; y++) {
      for (int x = 0; x < bar_w; x++) {
        float rel = (float)x / bar_w;
        uint16_t color;
        if (rel > 0.7f) color = 0xF800;
        else if (rel > 0.4f) color = 0xFFE0;
        else color = 0x07E0;

        int x_draw = WIDTH - 1 - x;
        int y_draw = HEIGHT - 1 - y;
        y_draw += 2;
        col[x_draw + y_draw * WIDTH] = color;
      }
    }
  }

  int samples_per_pixel = SAMPLES / HEIGHT;

  for (int y = 0; y < HEIGHT; y++) {

    int32_t sum = 0;
    for (int i = 0; i < samples_per_pixel; i++) sum += s_buffer[y * samples_per_pixel + i];
    
    int16_t avg = sum / samples_per_pixel;
    int x_pos = OFFSET + (avg * (WIDTH - 1)) / RESPON;
    if (x_pos < 0) x_pos = 0;
    if (x_pos >= WIDTH) x_pos = WIDTH - 1;

    int x_draw = WIDTH - 1 - x_pos;
    int y_draw = HEIGHT - 1 - y;
    col[x_draw + y_draw * WIDTH] = 0xFFFF;

  }

  st7735_start_pixels(pio, sm);

  for (int i = 0; i < SCR; i++) {

    uint16_t c = col[i];
    st7735_lcd_put(pio, sm, c >> 8);
    st7735_lcd_put(pio, sm, c & 0xff);

  }

}