// Belousov-Zhabotinsky reaction //

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
  
  float adjust = 1.2f;
  float a [WIDTH][HEIGHT][2];
  float b [WIDTH][HEIGHT][2];
  float c [WIDTH][HEIGHT][2];

  int p = 0, q = 1;
  int i,j;

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

  adjust = randomf(0.75f, 1.35f);

  for (int y = 0; y < HEIGHT; y++) {

    for (int x = 0; x < WIDTH; x++) {

      a[x][y][0] = randomf(0.0f, 1.0f);
      b[x][y][0] = randomf(0.0f, 1.0f);
      c[x][y][0] = randomf(0.0f, 1.0f);
      
    }
  }
}

void loop() {
  
  for (int y = 0; y < HEIGHT; y++) {

    for (int x = 0; x < WIDTH; x++) {

      float c_a = 0.0f;
      float c_b = 0.0f;
      float c_c = 0.0f;

      for (i = x - 1; i <= x+1; i++) {

        for (j = y - 1; j <= y+1; j++) {

          c_a += a[(i+WIDTH)%WIDTH][(j+HEIGHT)%HEIGHT][p];
          c_b += b[(i+WIDTH)%WIDTH][(j+HEIGHT)%HEIGHT][p];
          c_c += c[(i+WIDTH)%WIDTH][(j+HEIGHT)%HEIGHT][p];

        }
      }

      c_a /= 9.0f;
      c_b /= 9.0f;
      c_c /= 9.0f;

      a[x][y][q] = constrain(c_a + c_a * (adjust * c_b - c_c), 0.0f, 1.0f);
      b[x][y][q] = constrain(c_b + c_b * (c_c - adjust * c_a), 0.0f, 1.0f);
      c[x][y][q] = constrain(c_c + c_c * (c_a - c_b), 0.0f, 1.0f);
      
      uint8_t coll = 128.0f * a[x][y][q];
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      
      col[((4*x)+(4*y)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
      col[((4*x)+(4*y)*FULLW)+1] = (uint8_t)(image) & 0xFF;

    }

  }
  
  if (p == 0) { p = 1; q = 0; } else { p = 0; q = 1; }

  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
