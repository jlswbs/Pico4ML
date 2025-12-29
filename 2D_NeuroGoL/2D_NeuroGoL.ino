// 2D neuron-spike base GoL cellular automata // 

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
    uint16_t potential;
    uint16_t spike_syn;
} Neuron;

#define NEURON_SPIKING(n)   ((n).spike_syn & 0x8000)
#define NEURON_SYNAPSE(n)   ((n).spike_syn & 0x7FFF)

#define SET_SPIKE(n)        ((n).spike_syn |= 0x8000)
#define CLEAR_SPIKE(n)      ((n).spike_syn &= 0x7FFF)

#define SET_SYNAPSE(n,s)    ((n).spike_syn = ((n).spike_syn & 0x8000) | (s))

Neuron grid[SCR];
Neuron new_grid[SCR];

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

static inline uint16_t neuron_to_color(Neuron *n) {
    uint16_t r = n->potential;
    uint16_t g = n->spike_syn;
    uint16_t b = Q16_ONE - g;

    return ((r >> 11) << 11) |
           ((g >> 10) << 5 ) |
           ((b >> 11));
}

void update_automaton() {
    const uint16_t thresh_high = TO_Q16(0.7f);
    const uint16_t thresh_low  = TO_Q16(0.3f);

    for (int y=0;y<HEIGHT;y++) {
        int yu = (y+HEIGHT-1)%HEIGHT;
        int yd = (y+1)%HEIGHT;

        for (int x=0;x<WIDTH;x++) {
            int xl=(x+WIDTH-1)%WIDTH;
            int xr=(x+1)%WIDTH;
            int idx = IDX(x,y);

            int ln = 0;
            uint32_t sumV = 0;

            int n[8] = {
                IDX(xl,yu), IDX(x,yu), IDX(xr,yu),
                IDX(xl,y),            IDX(xr,y),
                IDX(xl,yd), IDX(x,yd), IDX(xr,yd)
            };

            for (int i=0;i<8;i++) {
                if (NEURON_SPIKING(grid[n[i]])) {
                    ln++;
                }
                sumV += grid[n[i]].potential;
            }

            uint16_t avgV = ln ? (sumV / 8) : 0;

            Neuron cur = grid[idx];
            Neuron nxt = cur;

            bool spiking = NEURON_SPIKING(cur);
            bool next_spike = spiking ? (ln==2 || ln==3) : (ln==3);

            if (spiking && cur.potential > thresh_high && ln>=1 && ln<=4) next_spike = true;
            if (spiking && cur.potential < thresh_low  && (ln<2 || ln>3)) next_spike = false;
            if (!spiking && avgV > TO_Q16(0.6f) && ln>=2 && ln<=3)       next_spike = true;

            if (next_spike) SET_SPIKE(nxt); else CLEAR_SPIKE(nxt);

            if (next_spike) {
                nxt.potential = MUL_Q16(cur.potential, TO_Q16(0.85f)) + TO_Q16(0.15f);
                uint16_t syn = NEURON_SYNAPSE(cur) + TO_Q16(0.01f);
                if (syn > 0x7FFF) syn = 0x7FFF;
                SET_SYNAPSE(nxt, syn);
            } else {
                nxt.potential = MUL_Q16(cur.potential, TO_Q16(0.7f));
                SET_SYNAPSE(nxt, MUL_Q16(NEURON_SYNAPSE(cur), TO_Q16(0.95f)));
            }
            nxt.potential = MUL_Q16(nxt.potential, TO_Q16(0.9f)) + MUL_Q16(avgV, TO_Q16(0.1f));
            if (nxt.potential > Q16_ONE) nxt.potential = Q16_ONE;
            new_grid[idx] = nxt;
        }
    }
    memcpy(grid, new_grid, sizeof(grid));
}

void init_automaton() {
    for (int i=0;i<SCR;i++) {
        grid[i].potential = rand() & 0xFFFF;
        grid[i].spike_syn = (rand() & 0x7FFF);
        if ((rand()%100) < 25) SET_SPIKE(grid[i]);
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
        uint16_t c = neuron_to_color(&grid[i]);
        st7735_lcd_put(pio, sm, c>>8);
        st7735_lcd_put(pio, sm, c&0xFF);
    }

}