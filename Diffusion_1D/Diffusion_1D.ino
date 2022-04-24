// 1D Diffusion reaction //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);}  

  uint8_t col[2*SCR];

  float A[HEIGHT]; 
  float I[HEIGHT];
  float D2A[HEIGHT]; 
  float D2I[HEIGHT];

  float p[6] = {0.5f, 2.0f, 2.0f, 2.0f, 1.0f, 0.0f};
  float a = 5.0f;
  float dt = 0.05f;

void rndrule(){

  p[0] = randomf(0.0f, 1.0f);
  p[1] = randomf(0.0f, 15.0f);
  p[2] = randomf(0.0f, 4.0f);
  p[3] = randomf(0.0f, 15.0f);
  p[4] = randomf(0.0f, 4.0f);
  p[5] = randomf(0.0f, 2.0f);

  a = randomf(1.9f, 7.9f);
   
  for(int i=0;i<HEIGHT; i++) {
    
    A[i] = randomf(0.0f, 1.0f);
    I[i] = randomf(0.0f, 1.0f);
    
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
 
}

void loop(){
  
  rndrule();

  for(int j=0;j<WIDTH; j++) {
    
    for(int i=1;i<HEIGHT-1; i++) {
      
      D2A[i] = A[i-1] + A[i+1] - 2.0f * A[i];
      D2I[i] = I[i-1] + I[i+1] - 2.0f * I[i];
 
    }
  
    D2A[0] = A[1] - A[0]; 
    D2I[0] = I[1] - I[0]; 

    for(int i=0;i<HEIGHT; i++) {
    
      A[i] = A[i] + dt * (a * A[i] *A[i] * A[i] / (I[i] * I[i]) + p[0] - p[1] * A[i] + p[2] * D2A[i]);
      I[i] = I[i] + dt * (A[i] * A[i] *A[i] - p[3] * I[i] + p[4] * D2I[i] + p[5]);  
        
      uint8_t coll = 250 - (50.0f * (A[i]+I[i]));
      uint16_t image = ST7735_COLOR565(coll, coll, coll);
      col[((2*j)+(2*i)*WIDTH)] = (uint8_t)(image >> 8) & 0xFF;
      col[((2*j)+(2*i)*WIDTH)+1] = (uint8_t)(image) & 0xFF;
      
    }

  }

  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

  delay(500);

}
