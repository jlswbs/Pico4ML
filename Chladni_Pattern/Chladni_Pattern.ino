// Chladni patterns //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   40
#define HEIGHT  80
#define FULLW   80
#define FULLH   160
#define SCR     (WIDTH*HEIGHT)
#define SCR2    (FULLW*FULLH)

  float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);} 
  
  uint8_t col[2*SCR2];
  
  float p[WIDTH][HEIGHT];
  float v[WIDTH][HEIGHT];
  float a = 0.01f;
  float b = 0.1f;
  float c = 8.0f;
    
void rndrule(){

  a = randomf(0.005f, 0.099f);
  b = randomf(0.049f, 0.199f);
  c = randomf(1.0f, 12.0f);

  for (int y = 0; y < HEIGHT; y++){
    for (int x = 0; x < WIDTH; x++){
      p[x][y] = 0;
      v[x][y] = 0;
    }
  }

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

  int frame = millis() / 32;

  v[WIDTH/2][HEIGHT/2] = 0;
  p[WIDTH/2][HEIGHT/2] = (sinf(frame * a) + sinf(frame * b)) * c;
  
  for (int y = 1; y < HEIGHT-1; y++) {
    for (int x = 1; x < WIDTH-1; x++) {
      v[x][y] += (p[x-1][y] + p[x+1][y] + p[x][y-1] + p[x][y+1]) * 0.25f - p[x][y];
    }
  }
  
  for (int y = 0; y < HEIGHT; y++){
    for (int x = 0; x < WIDTH; x++){
      p[x][y] += v[x][y];
      uint8_t coll = 255.0f * fabs(constrain(v[x][y], -1.0f, 1.0f));
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((4*x)+(4*y)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
      col[((4*x)+(4*y)*FULLW)+1] = (uint8_t)(image) & 0xFF;
    }
  }
  
  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
