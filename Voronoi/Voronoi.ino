// Voronoi distribution (cell noise) //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   40
#define HEIGHT  80
#define FULLW   80
#define FULLH   160
#define SCR     (WIDTH*HEIGHT)
#define SCR2    (FULLW*FULLH)
 
  uint8_t col[2*SCR2];

#define PARTICLES 10

  float mindist = 0;
  int x[PARTICLES];
  int y[PARTICLES];
  int dx[PARTICLES];
  int dy[PARTICLES];

float distance(int x1, int y1, int x2, int y2) { return sqrtf(powf(x2 - x1, 2.0f) + powf(y2 - y1, 2.0f)); }
    
void rndrule(){

  for (int i=0; i<PARTICLES; i++) {
    
    x[i] = rand()%WIDTH;
    y[i] = rand()%HEIGHT;
    dx[i] = rand()%4;
    dy[i] = rand()%4;
    
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

void setup() {
  
  seed_random_from_rosc();

  ST7735_Init();
  ST7735_FillScreen(ST7735_BLACK);

  rndrule();
 
}

void loop(){

  for (int j=0; j<HEIGHT; j++) {
    
    for (int i=0; i<WIDTH; i++) {
      
      mindist = WIDTH;
      
      for (int p=0; p<PARTICLES; p++) {
        if (distance(x[p], y[p], i, j) < mindist) mindist = distance(x[p], y[p], i, j);
        if (distance(x[p]+WIDTH, y[p], i, j) < mindist) mindist = distance(x[p]+WIDTH, y[p], i, j);
        if (distance(x[p]-WIDTH, y[p], i, j) < mindist) mindist = distance(x[p]-WIDTH, y[p], i, j);
      }
      
      uint8_t coll = 6.0f * mindist;
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((4*i)+(4*j)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
      col[((4*i)+(4*j)*FULLW)+1] = (uint8_t)(image) & 0xFF;

    }
  }
  
  for (int p=0; p<PARTICLES; p++) {
    x[p] += dx[p];
    y[p] += dy[p];
    x[p] = x[p] % WIDTH;
    y[p] = y[p] % HEIGHT;
  }  

  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
