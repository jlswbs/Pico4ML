// Prominence math patterns //

#include "hardware/structs/rosc.h"
#include "st7735.h"

#define WIDTH   80
#define HEIGHT  160
#define SCR     (WIDTH*HEIGHT)

#define ITER    10
#define N       500
#define M       5 

float randomf(float minf, float maxf) {return minf + (rand()%(1UL << 31))*(maxf - minf) / (1UL << 31);}

  uint8_t col[2*SCR];
  uint16_t coll = rand();
  float h = 0.5f;
  float ds = -1.0f;
  float X[N];
  float Y[N];
  float T[M];
  float dx, dy, a, s, k;
  int ct;
  

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

  for (int I = 0 ; I < N ; I++){
    X[I] = randomf(0.0f , WIDTH);
    Y[I] = HEIGHT;
  }
  
  for (int II = 0 ; II < M ; II++) T[II] = randomf(10.0f, 40.0f);
  
}

void loop(){

  if(ct == ITER) {
    
    coll = rand();
    ct = 0;
    
  }    

  for (int I = 0; I < N; I++){

    for (int II = 0; II < M; II++){
      
      a = 2.0f * PI * II / M;
      k = (X[I] * cosf(a)) - ((Y[I]+s) * sinf(a));
      k = h * sinf(k/T[II]);
      float kdx = -k * sinf(-a);
      float kdy = +k * cosf(-a);
      dx = dx + kdx;
      dy = dy + kdy;
      
    }
    
    X[I] = X[I] + dx;
    Y[I] = Y[I] + dy;
    dx = 0;
    dy = 0;
    
    if(X[I] < 0 || X[I] > WIDTH || Y[I] > HEIGHT || Y[I] < 0 || randomf(0.0f, 1000.0f) < 1) {
      
      Y[I] = 0.0f;
      X[I] = randomf(0.0f, WIDTH);   
    
    }
    
    a = PI * X[I] / WIDTH;
    k = Y[I] + 5.0f;

    int ax = (WIDTH/2) + (k * cosf(a));   
    int ay = (HEIGHT/2) + (k * sinf(a));
    int by = (HEIGHT/2) - (k * sinf(a)); 
    
    if(ax>0 && ax<WIDTH && ay>0 && ay<HEIGHT) {      
    
      col[((2*ax)+(2*ay)*WIDTH)] = (uint8_t)(coll >> 8) & 0xFF;
      col[((2*ax)+(2*ay)*WIDTH)+1] = (uint8_t)(coll) & 0xFF;
      col[((2*ax)+(2*by)*WIDTH)] = (uint8_t)(coll >> 8) & 0xFF;
      col[((2*ax)+(2*by)*WIDTH)+1] = (uint8_t)(coll) & 0xFF;                

    }
  
  }
  
  s = s + ds;
  ct++;
  
  ST7735_DrawImage(0, 0, WIDTH, HEIGHT, col);

}
