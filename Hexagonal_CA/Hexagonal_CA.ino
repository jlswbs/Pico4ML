// Hexagonal cellular automata //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

  uint8_t col[2*SCR];

  uint16_t image;
  int8_t p[WIDTH][HEIGHT];
  int8_t v[WIDTH][HEIGHT];


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
}

void setup() {
  
  seed_random_from_rosc();

  ST7735_Init();
  ST7735_FillScreen(ST7735_BLACK);
 
}

void loop(){

  int cnt = 0;

  for (int y = 0; y < HEIGHT; y++){
    for (int x = 0; x < WIDTH; x++){
      p[x][y] = 1;
      v[x][y] = 1;
    }
  }

  p[WIDTH/2][HEIGHT/2] = -1;

  while(cnt < 133){

    for (int x = 1; x < WIDTH-1; x++){
      
      for (int y = 1; y < HEIGHT-1; y+=2) v[x][y] *= p[x][y] * p[x-1][y] * p[x+1][y] * p[x][y-1] * p[x][y+1] * p[x+1][y-1] * p[x+1][y+1];
      for (int y = 2; y < HEIGHT-1; y+=2) v[x][y] *= p[x][y] * p[x-1][y] * p[x+1][y] * p[x][y-1] * p[x][y+1] * p[x-1][y+1] * p[x-1][y-1];

    }

    for (int y = 1; y < HEIGHT-1; y++){
      
      for (int x = 1; x < WIDTH-1; x++){
        
        p[x][y] *= v[x][y];
        if (p[x][y] == -1) image = ST7735_WHITE;
        else image = ST7735_BLACK;
        col[((2*x)+(2*y)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
        col[((2*x)+(2*y)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
        
      }
    }

    ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);
    cnt++;

  }

}
