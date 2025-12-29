// 2D LC resonant base GoL cellular automata // 

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
  1,20,0x01, 1,10,0x11, 2,2,0x3A,0x05,
  2,0,0x36,0x08,
  5,0,0x2A,0x00,0x18,0x00,0x67,
  5,0,0x2B,0x00,0x00,0x00,0x9F,
  1,2,0x20, 1,2,0x13, 1,2,0x29, 0
};

#define Q16_ONE   65535u
#define TO_Q16(x) ((uint16_t)((x)*65535.0f+0.5f))
#define MUL_Q16(a,b) ((uint32_t)(a)*(uint32_t)(b)>>16)
#define CLAMP16(x) if((x)>Q16_ONE)(x)=Q16_ONE; else if((int32_t)(x)<0)(x)=0

typedef struct {
    int16_t V;
    int16_t I;
    uint16_t energy;
    uint16_t flags;
} LCCell;

#define LC_ACTIVE(c) ((c).flags & 0x8000)
#define SET_LC(c)    ((c).flags |= 0x8000)
#define CLR_LC(c)    ((c).flags &= 0x7FFF)

LCCell grid[SCR];
LCCell new_grid[SCR];

#define IDX(x,y) ((y)*WIDTH + (x))

static inline void lcd_set_dc_cs(bool dc,bool cs){
    sleep_us(1);
    gpio_put_masked((1u<<PIN_DC)|(1u<<PIN_CS),
        (!!dc<<PIN_DC)| (!!cs<<PIN_CS));
    sleep_us(1);
}

static inline void lcd_write_cmd(PIO pio,uint sm,
                                 const uint8_t *cmd,size_t cnt){
    st7735_lcd_wait_idle(pio, sm);
    lcd_set_dc_cs(0,0);
    st7735_lcd_put(pio, sm, *cmd++);
    if(cnt>=2){
        lcd_set_dc_cs(1,0);
        for(size_t i=0;i<cnt-1;i++)
            st7735_lcd_put(pio, sm, *cmd++);
    }
    lcd_set_dc_cs(1,1);
}

static inline void lcd_init(PIO pio,uint sm,const uint8_t *seq){
    const uint8_t *cmd=seq;
    while(*cmd){
        lcd_write_cmd(pio, sm, cmd+2, *cmd);
        sleep_ms(*(cmd+1)*5);
        cmd+=*cmd+2;
    }
}

static inline void st7735_start_pixels(PIO pio,uint sm){
    uint8_t c=0x2C;
    lcd_write_cmd(pio, sm, &c, 1);
    lcd_set_dc_cs(1,0);
}

static inline void seed_random_from_rosc(){
    uint32_t r=0;
    volatile uint32_t *rnd=
      (uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
    for(int i=0;i<32;i++){
        while(((*rnd)&1)==(((*rnd)>>1)&1));
        r=(r<<1)|((*rnd)&1);
    }
    srand(r);
}

static inline uint16_t lc_to_color(LCCell *c){
    uint16_t r = abs(c->V)<<1;
    uint16_t g = abs(c->I)<<1;
    uint16_t b = LC_ACTIVE(*c) ? Q16_ONE : (c->energy>>1);

    return ((r>>11)<<11)|((g>>10)<<5)|((b>>11));
}

void update_automaton(){

    const int16_t L_INV = TO_Q16(0.85f);
    const int16_t C_INV = TO_Q16(0.9f);
    const int16_t R     = TO_Q16(0.5f);
    const int16_t K     = TO_Q16(0.12f);

    for(int y=0;y<HEIGHT;y++){
        int yu=(y+HEIGHT-1)%HEIGHT;
        int yd=(y+1)%HEIGHT;

        for(int x=0;x<WIDTH;x++){
            int xl=(x+WIDTH-1)%WIDTH;
            int xr=(x+1)%WIDTH;
            int i=IDX(x,y);

            int n[4]={
                IDX(xl,y), IDX(xr,y),
                IDX(x,yu), IDX(x,yd)
            };

            int32_t sumV=0;
            for(int k=0;k<4;k++) sumV+=grid[n[k]].V;
            int16_t avgV=sumV>>2;

            LCCell cur=grid[i];
            LCCell nxt=cur;

            int32_t dI =
                MUL_Q16(cur.V, L_INV) -
                MUL_Q16(cur.I, R);

            nxt.I = cur.I + (dI>>8);

            int32_t dV =
                MUL_Q16(nxt.I, C_INV) +
                MUL_Q16(avgV-cur.V, K);

            nxt.V = cur.V + (dV>>8);

            nxt.energy =
                (abs(nxt.V) + abs(nxt.I))<<1;
            CLAMP16(nxt.energy);

            if(nxt.energy > TO_Q16(0.6f))
                SET_LC(nxt);
            else if(nxt.energy < TO_Q16(0.2f))
                CLR_LC(nxt);

            new_grid[i]=nxt;
        }
    }
    memcpy(grid,new_grid,sizeof(grid));
}

void init_automaton(){
    for(int i=0;i<SCR;i++){
        grid[i].V = (rand()%65536)-32768;
        grid[i].I = (rand()%65536)-32768;
        grid[i].energy=0;
        grid[i].flags=0;
    }
}

void setup(){

    offset = pio_add_program(pio, &st7735_lcd_program);
    st7735_lcd_program_init(pio, sm, offset, PIN_DIN, PIN_CLK, SERIAL_CLK_DIV);

    gpio_init(PIN_CS);
    gpio_init(PIN_DC);
    gpio_init(PIN_RESET);
    gpio_set_dir(PIN_CS,GPIO_OUT);
    gpio_set_dir(PIN_DC,GPIO_OUT);
    gpio_set_dir(PIN_RESET,GPIO_OUT);
    gpio_put(PIN_CS,1);
    gpio_put(PIN_RESET,1);

    lcd_init(pio, sm, st7735_init_seq);

    seed_random_from_rosc();

    init_automaton();

}

void loop(){

    update_automaton();

    st7735_start_pixels(pio, sm);

    for(int i=0;i<SCR;i++){
        uint16_t c = lc_to_color(&grid[i]);
        st7735_lcd_put(pio, sm, c>>8);
        st7735_lcd_put(pio, sm, c&0xFF);
    }

}