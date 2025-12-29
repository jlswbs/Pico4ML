// 2D memristor base GoL cellular automata // 

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

#define Q16_ONE   65535u
#define TO_Q16(x) ((uint16_t)((x) * 65535.0f + 0.5f))
#define MUL_Q16(a,b) ((uint32_t)(a) * (uint32_t)(b) >> 16)

typedef struct {
    uint16_t energy;
    uint16_t memory_alive;
} Cell;

#define CELL_ALIVE(c)   ((c).memory_alive & 0x8000)
#define CELL_MEMORY(c)  ((c).memory_alive & 0x7FFF)

#define SET_ALIVE(c)    ((c).memory_alive |= 0x8000)
#define CLEAR_ALIVE(c)  ((c).memory_alive &= 0x7FFF)

#define SET_MEMORY(c,m) ((c).memory_alive = ((c).memory_alive & 0x8000) | (m))

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

static inline void lcd_init(PIO pio, uint sm, const uint8_t *init_seq) {
    const uint8_t *cmd = init_seq;
    while (*cmd) {
        lcd_write_cmd(pio, sm, cmd+2, *cmd);
        sleep_ms(*(cmd+1)*5);
        cmd += *cmd + 2;
    }
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

uint16_t cell_to_color(Cell *c) {
    float energy = c->energy / 65535.0f;
    float memory = CELL_MEMORY(*c) / 32767.0f;
    bool alive = CELL_ALIVE(*c);

    float hue = memory * 360.0f;
    float sat = 0.7f + 0.3f * energy;
    float val = alive ? (0.3f + 0.7f * energy) : 0.15f;

    while (hue >= 360.0f) hue -= 360.0f;

    float hh = hue / 60.0f;
    int i = (int)hh;
    float f = hh - i;

    float p = val * (1.0f - sat);
    float q = val * (1.0f - sat * f);
    float t = val * (1.0f - sat * (1.0f - f));

    float r,g,b;
    switch (i) {
        case 0: r=val; g=t; b=p; break;
        case 1: r=q; g=val; b=p; break;
        case 2: r=p; g=val; b=t; break;
        case 3: r=p; g=q; b=val; break;
        case 4: r=t; g=p; b=val; break;
        default:r=val; g=p; b=q; break;
    }

    return ((int)(r*31)<<11) | ((int)(g*63)<<5) | (int)(b*31);
}

void update_automaton() {
    for (int y=0;y<HEIGHT;y++) {
        int yu = (y+HEIGHT-1)%HEIGHT;
        int yd = (y+1)%HEIGHT;

        for (int x=0;x<WIDTH;x++) {
            int xl=(x+WIDTH-1)%WIDTH;
            int xr=(x+1)%WIDTH;
            int idx = IDX(x,y);

            int ln = 0;
            uint32_t sumE = 0;

            int n[8] = {
                IDX(xl,yu), IDX(x,yu), IDX(xr,yu),
                IDX(xl,y),            IDX(xr,y),
                IDX(xl,yd), IDX(x,yd), IDX(xr,yd)
            };

            for (int i=0;i<8;i++) {
                if (CELL_ALIVE(grid[n[i]])) {
                    ln++;
                    sumE += grid[n[i]].energy;
                }
            }

            uint16_t avgE = ln ? (sumE / ln) : 0;

            Cell cur = grid[idx];
            Cell nxt = cur;

            bool alive = CELL_ALIVE(cur);
            bool next_alive = alive ? (ln==2||ln==3) : (ln==3);

            if (alive && cur.energy > TO_Q16(0.7f) && ln>=1 && ln<=4) next_alive=true;
            if (alive && cur.energy < TO_Q16(0.3f) && (ln<2||ln>3)) next_alive=false;
            if (!alive && avgE > TO_Q16(0.6f) && ln>=2 && ln<=3) next_alive=true;

            if (next_alive) SET_ALIVE(nxt); else CLEAR_ALIVE(nxt);

            if (next_alive) {
                nxt.energy = MUL_Q16(cur.energy, TO_Q16(0.85f)) + TO_Q16(0.15f);
                uint16_t mem = CELL_MEMORY(cur) + TO_Q16(0.01f);
                if (mem > 0x7FFF) mem = 0x7FFF;
                SET_MEMORY(nxt, mem);
            } else {
                nxt.energy = MUL_Q16(cur.energy, TO_Q16(0.7f));
                SET_MEMORY(nxt, MUL_Q16(CELL_MEMORY(cur), TO_Q16(0.95f)));
            }

            nxt.energy = MUL_Q16(nxt.energy, TO_Q16(0.9f)) + MUL_Q16(avgE, TO_Q16(0.1f));
            if (nxt.energy > Q16_ONE) nxt.energy = Q16_ONE;

            new_grid[idx] = nxt;
        }
    }
    memcpy(grid, new_grid, sizeof(grid));
}

void init_automaton() {
    for (int i=0;i<SCR;i++) {
        grid[i].energy = rand() & 0xFFFF;
        grid[i].memory_alive = (rand()&0x7FFF);
        if ((rand()%100) < 25) SET_ALIVE(grid[i]);
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

    lcd_init(pio, sm, st7735_init_seq);

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