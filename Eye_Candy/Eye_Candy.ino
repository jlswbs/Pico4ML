// Eye candy patterns //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   40
#define HEIGHT  80
#define FULLW   80
#define FULLH   160
#define SCR     (WIDTH*HEIGHT)
#define SCR2    (FULLW*FULLH)
#define NUMS    32

float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);} 
  
  uint8_t col[2*SCR2];
  
  float p[WIDTH][HEIGHT];
  float v[WIDTH][HEIGHT];
  float a[WIDTH][HEIGHT];
    
void rndrule(){

  for (int i = 0; i < NUMS; i++) v[1+rand()%(WIDTH-2)][1+rand()%(HEIGHT-2)] = randomf(-TWO_PI, TWO_PI);

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
}

void setup(){
  
  seed_random_from_rosc();

  ST7735_Init();
  ST7735_FillScreen(ST7735_BLACK);

  rndrule();
  
}

void loop(){

  for (int y = 1; y < HEIGHT-1; y++) {
    for (int x = 1; x < WIDTH-1; x++) {
      a[x][y] = (v[x-1][y] + v[x+1][y] + v[x][y-1] + v[x][y+1]) * 0.25f - v[x][y];
    }
  }
  
  for (int y = 0; y < HEIGHT; y++){
    for (int x = 0; x < WIDTH; x++){
      v[x][y] += a[x][y];
      p[x][y] += v[x][y];
      uint8_t coll = 128 + (96.0f * sinf(p[x][y]));
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((4*x)+(4*y)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
      col[((4*x)+(4*y)*FULLW)+1] = (uint8_t)(image) & 0xFF;
    }
  }
  
  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
