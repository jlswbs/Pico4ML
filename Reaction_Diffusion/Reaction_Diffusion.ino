// Chemical reaction diffusion //

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

  float U[WIDTH][HEIGHT];
  float V[WIDTH][HEIGHT];
  float dU[WIDTH][HEIGHT];
  float dV[WIDTH][HEIGHT];
  float offsetW[WIDTH][2];
  float offsetH[HEIGHT][2];

  float diffU = 0.19f;
  float diffV = 0.09f;
  float paramF = 0.062f;
  float paramK = 0.062f;

void rndrule(){

  diffU = randomf(0.12f, 0.25f);
  diffV = randomf(0.081f, 0.099f);
    
  for (int j = 0; j < HEIGHT; j++) {
    for (int i = 0; i < WIDTH; i++){
      U[i][j] = 0.5f * (1.0f + randomf(-1.0f, 1.0f));
      V[i][j] = 0.25f * (1.0f + randomf(-1.0f, 1.0f));
    }
  }

  for (int i = 1; i < WIDTH-1; i++) {
    offsetW[i][0] = i-1;
    offsetW[i][1] = i+1;
  }
 
  offsetW[0][0] = WIDTH-1;
  offsetW[0][1] = 1;
  offsetW[WIDTH-1][0] = WIDTH-2;
  offsetW[WIDTH-1][1] = 0;

  for (int i = 1; i < HEIGHT-1; i++) {
    offsetH[i][0] = i-1;
    offsetH[i][1] = i+1;
  }
 
  offsetH[0][0] = HEIGHT-1;
  offsetH[0][1] = 1;   
  offsetH[HEIGHT-1][0] = HEIGHT-2;
  offsetH[HEIGHT-1][1] = 0;

}

void timestep(float F, float K, float diffU, float diffV) {
      
  for (int j = 0; j < HEIGHT; j++) {
    for (int i = 0; i < WIDTH; i++) {
           
      float u = U[i][j];
      float v = V[i][j];
           
      int left = offsetW[i][0];
      int right = offsetW[i][1];
      int up = offsetH[j][0];
      int down = offsetH[j][1];
           
      float uvv = u*v*v;    
        
      float lapU = (U[left][j] + U[right][j] + U[i][up] + U[i][down] - 4.0f * u);
      float lapV = (V[left][j] + V[right][j] + V[i][up] + V[i][down] - 4.0f * v);
           
      dU[i][j] = diffU * lapU - uvv + F * (1.0f - u);
      dV[i][j] = diffV * lapV + uvv - (K + F) * v;
    }
  }
       
  for (int j = 0; j < HEIGHT; j++){
    for (int i= 0; i < WIDTH; i++) {
      U[i][j] += dU[i][j];
      V[i][j] += dV[i][j];
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

  timestep(paramF, paramK, diffU, diffV);

  for (int j = 0; j < HEIGHT; j++){
    for (int i = 0; i < WIDTH; i++){
      
      uint8_t coll = 128.0f * U[i][j];
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((4*i)+(4*j)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
      col[((4*i)+(4*j)*FULLW)+1] = (uint8_t)(image) & 0xFF;

    }
  }

  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
