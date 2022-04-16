// Chemical diffusion reaction //

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

  float u[WIDTH+2][HEIGHT+2];
  float v[WIDTH+2][HEIGHT+2];
  float u1[WIDTH+2][HEIGHT+2];
  float v1[WIDTH+2][HEIGHT+2];

  float dt = 1.2f, h = 0.1f, h2 = h*h;
  float a = 0.024f, b = 0.078f;
  float cu = 0.002f, cv = 0.001f;

void rndrule(){

  a = randomf(0.021f, 0.027f);
  b = randomf(0.074f, 0.081f);

  for (int j = 0; j < HEIGHT; j++){
    for (int i = 0; i < WIDTH; i++){
      u[i][j] = 0.5f + randomf(-0.5f, 0.5f);
      v[i][j] = 0.125f + randomf(-0.25f, 0.25f);
    }
  }

}

void update(){
  
  for (int j = 0; j < HEIGHT; j++){
    for (int i = 0; i < WIDTH; i++){
    
      float Du = (u[i+1][j] + u[i][j+1] + u[i-1][j] + u[i][j-1] - 4.0f * u[i][j]) / h2;
      float Dv = (v[i+1][j] + v[i][j+1] + v[i-1][j] + v[i][j-1] - 4.0f * v[i][j]) / h2;
      float f = - u[i][j] * sq(v[i][j]) + a * (1.0f - u[i][j]);
      float g = u[i][j] * sq(v[i][j]) - b * v[i][j];
      u1[i][j] = u[i][j] + (cu * Du + f) * dt;
      v1[i][j] = v[i][j] + (cv * Dv + g) * dt;
      
    }
  }

  for (int j = 0; j < HEIGHT; j++){
    for (int i = 0; i < WIDTH; i++){  
      u[i][j] = u1[i][j];
      v[i][j] = v1[i][j];
    }
  }
  
}

void boundary(){

  for (int j = 0; j < HEIGHT; j++){
    for (int i = 0; i < WIDTH; i++){
      
      u[i][0] = u[i][HEIGHT];
      u[i][HEIGHT+1] = u[i][1];
      u[0][j] = u[WIDTH][j];
      u[WIDTH+1][j] = u[1][j];
      v[i][0] = v[i][HEIGHT];
      v[i][HEIGHT+1] = v[i][j];
      v[0][j] = v[WIDTH][j];
      v[WIDTH+1][j] = v[1][j];
      
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

void setup() {
  
  seed_random_from_rosc();

  ST7735_Init();
  ST7735_FillScreen(ST7735_BLACK);

  rndrule();
 
}

void loop(){  

  for (int y = 0; y < HEIGHT; y++){
    for (int x = 0; x < WIDTH; x++){
      
      uint8_t coll = 128.0f * u[x][y];
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((4*x)+(4*y)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
      col[((4*x)+(4*y)*FULLW)+1] = (uint8_t)(image) & 0xFF;

    }
  }

  boundary();
  update();

  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
