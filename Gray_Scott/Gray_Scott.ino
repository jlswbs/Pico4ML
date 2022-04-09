// Gray-Scott reaction diffusion //

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

  float a[WIDTH][HEIGHT];
  float aNext[WIDTH][HEIGHT];
  float b[WIDTH][HEIGHT];
  float bNext[WIDTH][HEIGHT];
  float temp[WIDTH][HEIGHT];
  float deltaT = 5.0f;
  float reactionRate = 0.49f;
  float aRate = 0.0385f;
  float bRate = 0.008f;
  float k = 0.031000046f;
  float F = 0.0069999984f;

  int iNext[WIDTH];
  int jNext[HEIGHT];
  int iPrev[WIDTH];
  int jPrev[HEIGHT];
    
void rndrule(){

  deltaT = randomf(1.2f, 5.0f);
  reactionRate = randomf(0.49f, 0.62f);
  aRate = randomf(0.029f, 0.049f);
  bRate = randomf(0.007f, 0.015f);

  for(int j=0;j<HEIGHT;++j){
    for(int i=0;i<WIDTH;++i){
      
      iNext[i] = (i+1)%WIDTH;
      iPrev[i] = (i-1+WIDTH)%WIDTH;
      jNext[j] = (j+1)%HEIGHT;
      jPrev[j] = (j-1+HEIGHT)%HEIGHT;
      
    }
  }

  for(int j=(HEIGHT/2)-8;j<(HEIGHT/2)+8;++j){
    for(int i=(WIDTH/2)-4;i<(WIDTH/2)+4;++i){
      
      a[i][j] = 0.5f + randomf(-0.5f, 0.5f);
      b[i][j] = 0.25f + randomf(-0.25f, 0.25f);

    }
  }

}

void diffusion(){
  
  for(int j=0;j<HEIGHT;++j){
    for(int i=0;i<WIDTH;++i){
      
      aNext[i][j] = a[i][j]+aRate*deltaT*
        (a[iNext[i]][j]+a[iPrev[i]][j]
        +a[i][jNext[j]]+a[i][jPrev[j]]
        -4*a[i][j]);
      
      bNext[i][j] = b[i][j]+bRate*deltaT*
        (b[iNext[i]][j]+b[iPrev[i]][j]
        +b[i][jNext[j]]+b[i][jPrev[j]]
        -4*b[i][j]);
    }
  }

  for(int j=0;j<HEIGHT;++j){
    for(int i=0;i<WIDTH;++i){  
  
      temp[i][j] = a[i][j];
      a[i][j] = aNext[i][j];
      aNext[i][j] = temp[i][j];
      temp[i][j] = b[i][j];
      b[i][j] = bNext[i][j];
      bNext[i][j] = temp[i][j];
    }
  }
}
  
void reaction(){
  
  float valA, valB;

  for(int j=0;j<HEIGHT;++j){
    for(int i=0;i<WIDTH;++i){
  
      valA = deltaT*reactionA(a[i][j],b[i][j]);
      valB = deltaT*reactionB(a[i][j],b[i][j]);
      a[i][j] += valA;
      b[i][j] += valB;

    }
  }
}
  
float reactionA(float aVal, float bVal){ return reactionRate*(-aVal*bVal*bVal+F*(1.0f-aVal)); }
float reactionB(float aVal, float bVal){ return reactionRate*(aVal*bVal*bVal-(F+k)*bVal); }

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

  diffusion();
  reaction();

  for(int j=0;j<HEIGHT;++j){
    for(int i=0;i<WIDTH;++i){
      
      uint8_t coll = 256.0f * a[i][j];
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
        col[((4*i)+(4*j)*FULLW)] = (uint8_t)(image >> 8) & 0xFF;
        col[((4*i)+(4*j)*FULLW)+1] = (uint8_t)(image) & 0xFF;
         
    }
  }
 
  ST7735_DrawImage(0, 0, FULLW, FULLH, col);

}
