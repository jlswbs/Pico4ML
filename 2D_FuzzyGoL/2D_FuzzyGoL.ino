// 2D fuzzy-logic base GoL cellular automata // 

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

#define Q16_ONE   65535u
#define TO_Q16(x) ((uint16_t)((x) * 65535.0f))
#define MUL_Q16(a,b) ((uint32_t)(a) * (uint32_t)(b) >> 16)

#define FUZZY_NEIGHBOR_GAIN     TO_Q16(0.60f)
#define FUZZY_SELF_DAMP         TO_Q16(0.62f)
#define FUZZY_EXCITE_UP         TO_Q16(0.05f)
#define FUZZY_EXCITE_DECAY      TO_Q16(0.97f)
#define FUZZY_EXCITE_INFLUENCE  TO_Q16(0.05f)

typedef struct {
    uint16_t mu_alive;
    uint16_t excite;
} Cell;

Cell grid[SCR];
Cell new_grid[SCR];

#define IDX(x,y) ((y)*WIDTH + (x))

static inline void lcd_set_dc_cs(bool dc, bool cs) {
    sleep_us(1);
    gpio_put_masked((1u<<PIN_DC)|(1u<<PIN_CS), (!!dc<<PIN_DC)| (!!cs<<PIN_CS));
    sleep_us(1);
}

static inline void lcd_write_cmd(PIO pio, uint sm, const uint8_t *cmd, size_t count) {
    st7735_lcd_wait_idle(pio, sm);
    lcd_set_dc_cs(0,0);
    st7735_lcd_put(pio, sm, *cmd++);
    if (count >= 2) {
        lcd_set_dc_cs(1,0);
        for (size_t i=0;i<count-1;i++) st7735_lcd_put(pio, sm, *cmd++);
    }
    lcd_set_dc_cs(1,1);
}

static inline void st7735_start_pixels(PIO pio, uint sm) {
    uint8_t cmd = 0x2C;
    lcd_write_cmd(pio, sm, &cmd, 1);
    lcd_set_dc_cs(1,0);
}

static inline void seed_random_from_rosc() {
    uint32_t r = 0;
    volatile uint32_t *rnd = (uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
    for (int i=0;i<32;i++) {
        while (((*rnd)&1) == (((*rnd)>>1)&1));
        r = (r<<1) | ((*rnd)&1);
    }
    srand(r);
}

static inline uint16_t cell_to_color(Cell *c) {
    uint16_t r = c->excite;
    uint16_t g = c->mu_alive;
    uint16_t b = Q16_ONE - g;

    return ((r >> 11) << 11) |
           ((g >> 10) << 5 ) |
           ((b >> 11));
}

void update_automaton() {
    for (int y=0;y<HEIGHT;y++) {
        int yu=(y+HEIGHT-1)%HEIGHT, yd=(y+1)%HEIGHT;
        for (int x=0;x<WIDTH;x++) {
            int xl=(x+WIDTH-1)%WIDTH, xr=(x+1)%WIDTH;
            int idx = IDX(x,y);

            uint32_t sum_mu = 0;
            int n[8] = {
                IDX(xl,yu), IDX(x,yu), IDX(xr,yu),
                IDX(xl,y),            IDX(xr,y),
                IDX(xl,yd), IDX(x,yd), IDX(xr,yd)
            };

            for (int i=0;i<8;i++) sum_mu += grid[n[i]].mu_alive;

            uint16_t avg_mu = sum_mu >> 3;

            Cell cur = grid[idx];
            Cell nxt = cur;

            nxt.excite = MUL_Q16(cur.excite, FUZZY_EXCITE_DECAY)
                       + MUL_Q16(cur.mu_alive, FUZZY_EXCITE_UP);

            int32_t delta =
                MUL_Q16(avg_mu, FUZZY_NEIGHBOR_GAIN) -
                MUL_Q16(cur.mu_alive, FUZZY_SELF_DAMP);

            nxt.mu_alive = cur.mu_alive + delta;
            nxt.mu_alive += MUL_Q16(nxt.excite, FUZZY_EXCITE_INFLUENCE);

            if ((int32_t)nxt.mu_alive < 0) nxt.mu_alive = 0;
            if (nxt.mu_alive > Q16_ONE) nxt.mu_alive = Q16_ONE;

            new_grid[idx] = nxt;
        }
    }
    memcpy(grid, new_grid, sizeof(grid));
}

void init_automaton() {
    for (int i=0;i<SCR;i++) {
        grid[i].mu_alive = rand() & 0xFFFF;
        grid[i].excite   = rand() & 0x7FFF;
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
    gpio_put(PIN_CS,1);
    gpio_put(PIN_RESET,1);

    seed_random_from_rosc();

    init_automaton();

}

void loop() {

    update_automaton();

    st7735_start_pixels(pio, sm);

    for (int i=0;i<SCR;i++) {
        uint16_t c = cell_to_color(&grid[i]);
        st7735_lcd_put(pio, sm, c>>8);
        st7735_lcd_put(pio, sm, c&0xFF);
    }

}