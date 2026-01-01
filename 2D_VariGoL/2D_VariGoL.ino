// 2D varicap base GoL cellular automata // 

#include "hardware/structs/rosc.h"
#include "st7735_lcd.pio.h"

#define PIN_DIN   11
#define PIN_CLK   10
#define PIN_CS    13
#define PIN_DC    9
#define PIN_RESET 7

PIO pio = pio0;
uint sm = 0;
uint offset;

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
    uint16_t voltage;
    uint16_t charge;
    uint16_t flags;
} Varicap;

#define VARI_CONDUCTING(v)   ((v).flags & 0x8000)
#define SET_CONDUCT(v)      ((v).flags |= 0x8000)
#define CLR_CONDUCT(v)      ((v).flags &= 0x7FFF)

Varicap grid[SCR];
Varicap new_grid[SCR];

#define IDX(x,y) ((y)*WIDTH + (x))

static inline void lcd_set_dc_cs(bool dc, bool cs) {
    sleep_us(1);
    gpio_put_masked((1u<<PIN_DC)|(1u<<PIN_CS),
        (!!dc<<PIN_DC)| (!!cs<<PIN_CS));
    sleep_us(1);
}

static inline void lcd_write_cmd(PIO pio, uint sm,
                                 const uint8_t *cmd, size_t count) {
    st7735_lcd_wait_idle(pio, sm);
    lcd_set_dc_cs(0,0);
    st7735_lcd_put(pio, sm, *cmd++);
    if (count >= 2) {
        lcd_set_dc_cs(1,0);
        for (size_t i=0;i<count-1;i++)
            st7735_lcd_put(pio, sm, *cmd++);
    }
    lcd_set_dc_cs(1,1);
}

static inline void lcd_init(PIO pio, uint sm,
                            const uint8_t *init_seq) {
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
    volatile uint32_t *rnd =
      (uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
    for (int i=0;i<32;i++) {
        while (((*rnd)&1) == (((*rnd)>>1)&1));
        r = (r<<1) | ((*rnd)&1);
    }
    srand(r);
}

static inline uint16_t varicap_C(uint16_t V) {
    uint32_t denom = Q16_ONE + MUL_Q16(V, TO_Q16(1.5f));
    return (uint32_t)Q16_ONE * Q16_ONE / denom;
}

static inline uint16_t varicap_to_color(Varicap *v) {
    uint16_t r = v->voltage;
    uint16_t g = varicap_C(v->voltage);
    uint16_t b = VARI_CONDUCTING(*v) ? Q16_ONE : (Q16_ONE - r);

    return ((r >> 11) << 11) |
           ((g >> 10) << 5 ) |
           ((b >> 11));
}

void update_automaton() {

    const uint16_t V_ON  = TO_Q16(0.20f);
    const uint16_t V_OFF = TO_Q16(0.50f);

    for (int y=0;y<HEIGHT;y++) {
        int yu = (y+HEIGHT-1)%HEIGHT;
        int yd = (y+1)%HEIGHT;

        for (int x=0;x<WIDTH;x++) {
            int xl=(x+WIDTH-1)%WIDTH;
            int xr=(x+1)%WIDTH;
            int idx = IDX(x,y);

            uint32_t sumV = 0;
            int act = 0;

            int n[8] = {
                IDX(xl,yu), IDX(x,yu), IDX(xr,yu),
                IDX(xl,y),            IDX(xr,y),
                IDX(xl,yd), IDX(x,yd), IDX(xr,yd)
            };

            for (int i=0;i<8;i++) {
                sumV += grid[n[i]].voltage;
                if (VARI_CONDUCTING(grid[n[i]])) act++;
            }

            uint16_t avgV = sumV >> 3;

            Varicap cur = grid[idx];
            Varicap nxt = cur;

            bool cond = VARI_CONDUCTING(cur);

            if (!cond && avgV > V_ON && act>=2 && act<=3)
                SET_CONDUCT(nxt);

            if (cond && (avgV < V_OFF || act<1 || act>4))
                CLR_CONDUCT(nxt);

            uint16_t C = varicap_C(cur.voltage);

            if (VARI_CONDUCTING(nxt)) {
                nxt.charge += MUL_Q16(C, TO_Q16(0.3f));
            } else {
                nxt.charge = MUL_Q16(cur.charge, TO_Q16(0.62f));
            }

            nxt.voltage =
              (uint32_t)nxt.charge * Q16_ONE / (C + 1);

            nxt.voltage =
                MUL_Q16(nxt.voltage, TO_Q16(0.80f)) +
                MUL_Q16(avgV,        TO_Q16(0.20f));

            if (nxt.voltage > Q16_ONE) nxt.voltage = Q16_ONE;

            new_grid[idx] = nxt;
        }
    }
    memcpy(grid, new_grid, sizeof(grid));
}

void init_automaton() {
    for (int i=0;i<SCR;i++) {
        grid[i].voltage = rand() & 0xFFFF;
        grid[i].charge  = rand() & 0xFFFF;
        grid[i].flags   = 0;
        if ((rand()%100) < 20) SET_CONDUCT(grid[i]);
    }
}

void setup() {

    offset = pio_add_program(pio, &st7735_lcd_program);
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
        uint16_t c = varicap_to_color(&grid[i]);
        st7735_lcd_put(pio, sm, c>>8);
        st7735_lcd_put(pio, sm, c&0xFF);
    }

}