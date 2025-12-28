// 2D GoL cellular automata with memristor color // 

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
#define SCR     (WIDTH * HEIGHT)
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

typedef struct {
    float state;
    float age;
} Memristor;

bool grid[SCR];
bool new_grid[SCR];
Memristor memristors[SCR];

#define IDX(x, y) ((y) * WIDTH + (x))

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
    for(int k = 0; k < 32; k++){
        while(1){
            random_bit = (*rnd_reg) & 1;
            if(random_bit != ((*rnd_reg) & 1)) break;
        }
        random = (random << 1) | random_bit;
    }
    srand(random);
}

uint16_t hsv_to_rgb565_fast(float h, float s, float v) {
    while (h >= 360.0f) h -= 360.0f;
    while (h < 0.0f) h += 360.0f;
    
    if (s == 0) {
        int val = (int)(v * 255);
        if (val > 255) val = 255;
        if (val < 0) val = 0;
        uint8_t v8 = val;
        return ((v8 & 0xF8) << 8) | ((v8 & 0xFC) << 3) | (v8 >> 3);
    }
    
    float hh = h / 60.0f;
    int i = (int)hh;
    float ff = hh - i;
    
    float p = v * (1.0f - s);
    float q = v * (1.0f - (s * ff));
    float t = v * (1.0f - (s * (1.0f - ff)));
    
    float r_f, g_f, b_f;
    
    switch (i) {
        case 0: r_f = v; g_f = t; b_f = p; break;
        case 1: r_f = q; g_f = v; b_f = p; break;
        case 2: r_f = p; g_f = v; b_f = t; break;
        case 3: r_f = p; g_f = q; b_f = v; break;
        case 4: r_f = t; g_f = p; b_f = v; break;
        default: r_f = v; g_f = p; b_f = q; break;
    }
    
    int r = (int)(r_f * 255);
    int g = (int)(g_f * 255);
    int b = (int)(b_f * 255);
    
    if (r > 255) r = 255; else if (r < 0) r = 0;
    if (g > 255) g = 255; else if (g < 0) g = 0;
    if (b > 255) b = 255; else if (b < 0) b = 0;
    
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void init_memristor(int idx) {
    memristors[idx].state = (float)rand() / (float)RAND_MAX;
    memristors[idx].age = 0;
}

void update_memristor(int idx, bool is_alive) {
    float target = is_alive ? 1.0f : 0.0f;
    memristors[idx].state = memristors[idx].state * 0.9f + target * 0.1f;
    if (is_alive) {
        memristors[idx].age += 1.0f;
        if (memristors[idx].age > 360.0f) memristors[idx].age -= 360.0f;
    } else {
        memristors[idx].age *= 0.95f;
    }
}

uint16_t get_memristor_color(int idx) {
    float hue = memristors[idx].state * 360.0f + memristors[idx].age;
    return hsv_to_rgb565_fast(hue, 0.8f, 0.3f + memristors[idx].state * 0.7f);
}

void init_game() {
    for (int idx = 0; idx < SCR; idx++) {
        grid[idx] = (rand() % 100) < 30;
        init_memristor(idx);
        update_memristor(idx, grid[idx]);
    }
}

void update_game() {
    for (int y = 0; y < HEIGHT; y++) {
        int y_up = (y - 1 + HEIGHT) % HEIGHT;
        int y_down = (y + 1) % HEIGHT;
        
        for (int x = 0; x < WIDTH; x++) {
            int idx = IDX(x, y);
            int x_left = (x - 1 + WIDTH) % WIDTH;
            int x_right = (x + 1) % WIDTH;
            
            int count = 0;
            
            if (grid[IDX(x_left, y_up)]) count++;
            if (grid[IDX(x, y_up)]) count++;
            if (grid[IDX(x_right, y_up)]) count++;
            
            if (grid[IDX(x_left, y)]) count++;
            if (grid[IDX(x_right, y)]) count++;
            
            if (grid[IDX(x_left, y_down)]) count++;
            if (grid[IDX(x, y_down)]) count++;
            if (grid[IDX(x_right, y_down)]) count++;
            
            bool alive = grid[idx];
            bool new_alive;
            
            if (alive) {
                new_alive = (count == 2 || count == 3);
            } else {
                new_alive = (count == 3);
            }
            
            new_grid[idx] = new_alive;
            
            update_memristor(idx, new_alive);
        }
    }
    
    memcpy(grid, new_grid, sizeof(grid));
}

void setup() {

  st7735_lcd_program_init(pio, sm, offset, PIN_DIN, PIN_CLK, SERIAL_CLK_DIV);
    
  gpio_init(PIN_CS); gpio_init(PIN_DC); gpio_init(PIN_RESET);
  gpio_set_dir(PIN_CS, GPIO_OUT); gpio_set_dir(PIN_DC, GPIO_OUT); gpio_set_dir(PIN_RESET, GPIO_OUT);
  gpio_put(PIN_CS, 1); gpio_put(PIN_RESET, 1);
    
  lcd_init(pio, sm, st7735_init_seq);
    
  seed_random_from_rosc();
    
  init_game();

}

void loop() {

  update_game();
    
  st7735_start_pixels(pio, sm);
    
  for (int idx = 0; idx < SCR; idx++) {
    uint16_t color = get_memristor_color(idx);
    st7735_lcd_put(pio, sm, color >> 8);
    st7735_lcd_put(pio, sm, color & 0xFF);
  }

}